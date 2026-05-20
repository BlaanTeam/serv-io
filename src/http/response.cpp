#include "response.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/sendfile.h>
#endif

using namespace std;

// Portable sendfile shim. The kernel ABI differs between platforms; the
// wrapper presents a uniform "send up to `count` bytes from offset, advance
// offset, return bytes-sent or -1 on hard error" contract.
static ssize_t sendfileTo(int sock, int filefd, off_t *offset, size_t count) {
#ifdef __APPLE__
	off_t len = (off_t)count;
	int   r = ::sendfile(filefd, sock, *offset, &len, NULL, 0);
	*offset += len;
	if (r == -1) {
		if ((errno == EAGAIN || errno == EINTR) && len > 0) return (ssize_t)len;
		return -1;
	}
	return (ssize_t)len;
#elif defined(__linux__)
	return ::sendfile(sock, filefd, offset, count);
#else
	char    buf[1 << 14];
	ssize_t want = (ssize_t)((count < sizeof(buf)) ? count : sizeof(buf));
	ssize_t got = pread(filefd, buf, (size_t)want, *offset);
	if (got <= 0) return got;
	ssize_t sent = ::send(sock, buf, (size_t)got, 0);
	if (sent > 0) *offset += sent;
	return sent;
#endif
}

Response::Response() {
	_keepAlive = true;
	_rangePhase = INIT_LENGTH;
	// unique_ptr members start null automatically
	_fileFd = -1;
	_filePos = 0;
	_fileLen = 0;
	_type = LENGTHED_RES;
	_isCustomStatusCode = false;
	setState(RES_INIT);
}

Response::Response(const short &statusCode, bool keepAlive) {
	_statusCode = statusCode;
	_keepAlive = keepAlive;
	// unique_ptr members start null automatically
	_fileFd = -1;
	_filePos = 0;
	_fileLen = 0;
	_type = LENGTHED_RES;
	_isCustomStatusCode = false;
	setState(RES_INIT);
}

Response::Response(const Response &copy) {
	// unique_ptr members start null automatically
	_fileFd = -1;
	_filePos = 0;
	_fileLen = 0;
	*this = copy;
}

Response &Response::operator=(const Response &rhs) {
	if (this != &rhs) {
		_keepAlive = rhs._keepAlive;
		_state = rhs._state;
		_type = rhs._type;
		_headers = rhs._headers;
		_isCustomStatusCode = rhs._isCustomStatusCode;
		// `_sender` and `_stream` are intentionally not copied — ownership
		// stays with the source; the destination needs a fresh strategy and
		// body stream chosen by its own setup pass.
	}
	return *this;
}

Response::~Response() {
	if (_fileFd >= 0) close(_fileFd);
}

void Response::init() {
	addHeader("Server", NAME "/" VERSION);
	addHeader("Date", getUTCDate());
	addHeader("Connection", _keepAlive ? "keep-alive" : "close");
	if (_keepAlive)
		addHeader("Keep-Alive", "timeout=" + (to_string((int)(TIMEOUT / 1e3))));
	else
		_headers.erase("Keep-Alive");
	addHeader("Accept-Ranges", "bytes");
}

void Response::prepare(void) {
	if (!_isCustomStatusCode)
		_statusStringCode = httpStatusCodes[_statusCode];
	_headerBuffer << HTTP_VERSION << " " << to_string(_statusCode) << " " << _statusStringCode << CRLF;

	for (const auto &entry : _headers) {
		for (const auto &value : entry.second)
			_headerBuffer << entry.first << ": " << value << CRLF;
	}
}

// Setters

void Response::setStatusCode(const short &statusCode) {
	_statusCode = statusCode;
}

void Response::setState(const int &state) {
	_state.replace((short)state);
}

void Response::setStream(iostream *stream) {
	_stream.reset(stream);
}

void Response::setConnectionStatus(bool keepAlive) {
	_keepAlive = keepAlive;
}

bool Response::keepAlive(void) const {
	return _keepAlive;
}

void Response::addHeader(const string &name, const string &value) {
	if (_state.any(RES_DONE | RES_BODY))
		return;
	// _headers[name] = value;
	_headers.add(name, value);
	setState(RES_HEADER);
}
void Response::send(const sockfd &fd) {
	if (!_sender)
		return;

	if (_state.any(RES_INIT | RES_HEADER)) {
		if (!_sender->prepareHeaders(*this))
			return;  // sender wants to defer — try again on the next poll
		prepare();
		::send(fd, _headerBuffer.str().c_str(), _headerBuffer.str().size(), 0);
		::send(fd, CRLF, 2, 0);
		setState(RES_BODY);
	}

	if (_state.any(RES_BODY))
		_sender->sendBody(*this, fd);
}

// Pick the body source for an error response: a configured error_page when
// present (and accessible), with sensible 404/403 fallbacks; otherwise the
// project's built-in error HTML.
static iostream *resolveErrorBody(int statusCode,
                                  MainContext<Type> *ctx,
                                  string &outContentType,
                                  bool isBuiltIn,
                                  int *nextFallback /* in/out: status to retry with */) {
	*nextFallback = 0;

	if (!ctx)
		return buildResponseBody(statusCode);

	const servio::Option<ErrorPage *> maybePage = ctx->errorPage(statusCode);
	if (maybePage.isNone())
		return buildResponseBody(statusCode);

	ErrorPage *errPage = maybePage.unwrap();
	if (errPage->exists()) {
		outContentType = mimeTypes.choiceMimeType(errPage->page);
		return new fstream(errPage->page.c_str(), ios::in);
	}

	// Configured error_page file is missing — fall back, preserving the
	// existing recursion behavior.
	if (isBuiltIn)        { *nextFallback = NOT_FOUND; return NULL; }
	if (errno == EACCES)  { *nextFallback = FORBIDDEN; return NULL; }
	return buildResponseBody(NOT_FOUND);
}

void Response::setupErrorResponse(const int &statusCode, MainContext<Type> *ctx, bool isBuiltIn) {
	string    contentType = mimeTypes["html"];
	int       fallback = 0;
	iostream *bodyStream = resolveErrorBody(statusCode, ctx, contentType, isBuiltIn, &fallback);
	if (fallback) {
		return setupErrorResponse(fallback, ctx, fallback == FORBIDDEN);
	}

	build()
		.asLengthed()
		.status(statusCode)
		.keepAlive(false)
		.contentType(contentType)
		.body(bodyStream)
		.apply();
}

void Response::setupRedirectResponse(Redirect *redir, MainContext<Type> *ctx) {
	redir->prepare(ctx);

	Builder b = build();
	b.asLengthed().status(redir->code);

	if (redir->isRedirect) {
		b.contentType(mimeTypes["html"])
		 .header("Location", redir->path)
		 .body(buildResponseBody(redir->code));
	} else {
		iostream *ss = new stringstream;
		*ss << redir->path;
		b.contentType(mimeTypes[""]).body(ss);
	}
	b.apply();
}

void Response::setupDirectoryListing(const string &path, const string &title) {
	build()
		.asLengthed()
		.status(OK)
		.contentType(mimeTypes["html"])
		.body(buildDirectoryListing(path, title))
		.apply();
}

bool Response::setupNormalResponse(const string &path) {
	const int fd = ::open(path.c_str(), O_RDONLY);
	if (fd < 0)
		return false;
	struct stat st;
	if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
		close(fd);
		return false;
	}
	_fileFd = fd;
	_filePos = 0;
	_fileLen = st.st_size;

	// Don't override `_type` here — it has already been set by
	// `extractRange()` to either LENGTHED_RES or RANGED_RES.
	build()
		.status(OK)
		.contentType(mimeTypes.choiceMimeType(path))
		.apply();
	return true;
}

void Response::parseHeaders(stringstream &ss) {
	string key;
	string value;

	if (ss.str() == CRLF || ss.str() == LF) {
		return changeState(RES_BODY);
	}
	getline(ss, key, ':');
	getline(ss, value, '\0');
	size_t n = 1;
	if (value.length() > 1 && value.substr(value.length() - 2) == CRLF)
		n = 2;
	if (!value.length())
		return;
	string tmp = value.substr(value.length() - n);
	if (value.length() < n + 1 || (tmp != LF && tmp != CRLF))
		return;
	value = value.substr(0, value.length() - n);
	_headers.add(key, value);
	return;
}

void Response::changeState(const int &state) {
	_state.replace((short)state);
}

void Response::setupCGIResponse(const int &fd, Request *req) {
	build().cgi(fd, req);
	// Note: no `.apply()` — the standard header bundle gets emitted later
	// by setupCGIBody once the CGI child's own headers are parsed.
}

bool Response::match(const int &state) const {
	return _state.any(state);
}

void Response::reset(void) {
	_headers.clear();
	_headerBuffer.str("");
	_headerBuffer.clear();
	_rangePhase = INIT_LENGTH;

	_stream.reset();
	_sender.reset();
	if (_fileFd >= 0) {
		close(_fileFd);
		_fileFd = -1;
	}
	_filePos = 0;
	_fileLen = 0;
	_isCustomStatusCode = false;
	_statusStringCode = "";

	_keepAlive = true;
	_type = LENGTHED_RES;

	setState(RES_INIT);
}

// private functions

void Response::setupLengthedBody() {
	if (_fileFd >= 0) {
		addHeader("Content-Length", to_string((long long)_fileLen));
		return;
	}
	if (!_stream)
		return;
	size_t seek = _stream->tellg();
	_stream->seekg(0, _stream->end);

	size_t contentLength = _stream->tellg() < 0 ? (streampos)0 : _stream->tellg();
	addHeader("Content-Length", to_string(contentLength));

	_stream->seekg(seek);
}

void Response::sendLengthedBody(const sockfd &fd) {
	if (_fileFd >= 0) {
		off_t  remaining = _fileLen - _filePos;
		if (remaining <= 0) {
			setState(RES_DONE);
			return;
		}
		size_t want = (remaining > (off_t)(1 << 16)) ? (size_t)(1 << 16) : (size_t)remaining;
		ssize_t sent = sendfileTo(fd, _fileFd, &_filePos, want);
		if (sent < 0 || _filePos >= _fileLen)
			setState(RES_DONE);
		return;
	}
	if (!_stream)
		return;
	char buff[(1 << 10)];

	_stream->read(buff, (1 << 10));
	::send(fd, buff, _stream->gcount(), 0);
	setState(_stream->eof() ? RES_DONE : _state.raw());
}

void Response::setupChunkedBody() {
	addHeader("Transfer-Encoding", "Chunked");
}

void Response::sendChunkedBody(const sockfd &fd) {
	if (!_stream)
		return;
	char buff[(1 << 10)];

	_stream->read(buff, (1 << 10));
	if (_stream->gcount() > 0) {
		_headerBuffer << hex << _stream->gcount() << CRLF;
		::send(fd, _headerBuffer.str().c_str(), _headerBuffer.str().size(), 0);
		::send(fd, buff, _stream->gcount(), 0);
		::send(fd, CRLF, 2, 0);
		_headerBuffer.str("");
		_headerBuffer.clear();
	}
	setState(_stream->eof() ? RES_DONE : _state.raw());
}

void Response::setupRangedBody() {
	RangeSpecifier range = _range.specifiers()[0];
	const size_t   fileSize = (_fileFd >= 0) ? (size_t)_fileLen : getFileSize(_stream.get());

	if (_fileFd >= 0)
		addHeader("Content-Length", to_string((long long)range.contentLength(fileSize)));
	else
		addHeader("Content-Length", to_string(range.contentLength(_stream.get())));
	setStatusCode(PARTIAL_CONTENT);

	if (range.type == NOL)
		range.rangeEnd = fileSize <= 0 ? 0 : fileSize - 1;
	else if (range.type == NOF) {
		range.rangeStart = range.rangeStart > fileSize ? 0 : fileSize - range.rangeEnd;
		range.rangeEnd = fileSize <= 0 ? 0 : fileSize - 1;
	}

	char buff[64];
	snprintf(buff, sizeof(buff), "bytes %ld-%ld/%ld", (long)range.rangeStart, (long)range.rangeEnd, (long)fileSize);
	addHeader("Content-Range", buff);
}

void Response::sendRangedBody(const sockfd &fd) {
	RangeSpecifier range = _range.specifiers()[0];
	const size_t   fileSize = (_fileFd >= 0) ? (size_t)_fileLen : getFileSize(_stream.get());

	// Normalize the range against the file size so the offsets we hand to
	// sendfile() / seekg() are always within [0, fileSize).
	size_t start, length;
	if (range.type == NOL) {
		start  = range.rangeStart;
		length = (fileSize > start) ? (fileSize - start) : 0;
	} else if (range.type == NOF) {
		const size_t suffix = (range.rangeEnd < fileSize) ? range.rangeEnd : fileSize;
		start  = fileSize - suffix;
		length = suffix;
	} else {
		start  = range.rangeStart;
		const size_t end = (range.rangeEnd < fileSize) ? range.rangeEnd : (fileSize ? fileSize - 1 : 0);
		length = (end >= start) ? (end - start + 1) : 0;
	}

	if (_rangePhase == INIT_LENGTH) {
		if (_fileFd >= 0) {
			_filePos = (off_t)start;
			_length  = (int)length;
		} else {
			_stream->seekg(start);
			_length  = (int)length;
		}
		_rangePhase = ONGOING_LENGTH;
	}

	if (_fileFd >= 0) {
		if (_length <= 0) {
			_rangePhase = DONE_LENGTH;
		} else {
			size_t  want = (_length > (1 << 16)) ? (size_t)(1 << 16) : (size_t)_length;
			ssize_t sent = sendfileTo(fd, _fileFd, &_filePos, want);
			if (sent < 0) {
				_rangePhase = DONE_LENGTH;
			} else {
				_length -= (int)sent;
				if (_length <= 0) _rangePhase = DONE_LENGTH;
			}
		}
	} else if (_length > (1 << 10))
		sendKiloByte(fd);
	else
		sendLessThanKiloByte(fd);

	if (_rangePhase & DONE_LENGTH)
		_state.replace(RES_DONE);
}

void Response::sendKiloByte(const sockfd &fd) {
	char buff[(1 << 10)];

	_stream->read(buff, (1 << 10));
	::send(fd, buff, _stream->gcount(), 0);
	_length -= _stream->gcount();
}

void Response::sendLessThanKiloByte(const sockfd &fd) {
	char buff[(1 << 10)];

	_stream->read(buff, _length);
	::send(fd, buff, _stream->gcount(), 0);
	_length -= _stream->gcount();
	_rangePhase = DONE_LENGTH;
}

bool Response::setupCGIBody() {
	string line = "";
	int    nbyte;
	char   chr;
	bool   got = false;
	while ((nbyte = read(_cgiFd, &chr, 1)) > 0) {
		line += chr;
		if (line.find(CRLF) != string::npos || line.find(LF) != string::npos) {
			stringstream ss(line);
			parseHeaders(ss);
			line = "";
			if (_state.any(RES_BODY))
				break;
		}
		got = true;
	}
	if (nbyte <= 0 && !got)
		return false;
	changeState(RES_HEADER);  // reset the state to header!
	const string contentType = _headers.get("Content-Type").unwrapOr(mimeTypes[""]);

	const servio::Option<string> status = _headers.get("Status");
	if (status.isSome()) {
		char buff[(1 << 10)];
		sscanf(status.unwrap().c_str(), "%hd %[^\n]1000s", &_statusCode, buff);
		_isCustomStatusCode = true;
		_statusStringCode = buff;
	}
	addHeader("Content-Type", contentType);
	init();
	return true;
}

void Response::sendCGIBody(const sockfd &fd) {
	char buff[(1 << 10)];
	int  nbyte = read(_cgiFd, buff, (1 << 10));
	if (nbyte <= 0) {
		setState(RES_DONE);
		close(_cgiFd);
	} else
		::send(fd, buff, nbyte, 0);
}

void Response::extractRange(Request &req) {
	_range = req.range();
	if (_range.empty()) {
		_sender.reset(new LengthedSender());
		_type = LENGTHED_RES;
	} else {
		_sender.reset(new RangedSender());
		_type = RANGED_RES;
	}
}

void Response::setupUploadResponse(LocationContext<Type> *location, Request *req) {
	build().upload(location, req);
	changeState(RES_HEADER);
}

bool Response::setupUploadBody() {
	if (!_req->match(REQ_DONE))
		return false;

	map<int, BodyFile> &bodyFiles = _req->bodyFiles();
	map<string, bool>   uploaded;  // filename -> success

	for (map<int, BodyFile>::iterator it = bodyFiles.begin(); it != bodyFiles.end(); ++it) {
		BodyFile &part = it->second;
		string    filename = part.clientFilename();
		if (filename.empty())
			filename = part.tmpPath();

		const string newPath = joinPath(_location->uploadStore(), filename);
		const bool   ok = (rename(part.tmpPath().c_str(), newPath.c_str()) == 0);
		uploaded[filename] = ok;
	}

	stringstream *html = new stringstream;
	*html << "<h1>Upload Successful</h1>";
	for (map<string, bool>::iterator it = uploaded.begin(); it != uploaded.end(); ++it)
		*html << "<br/><span>" << it->first << " -- "
		      << (it->second ? "Uploaded" : "Not Uploaded") << "</span>\n";
	_stream.reset(html);

	addHeader("Content-Length", to_string(getFileSize(_stream.get())));
	return true;
}

void Response::sendUploadBody(const sockfd &fd) {
	if (!_stream)
		return;
	char buff[(1 << 10)];

	_stream->read(buff, (1 << 10));
	::send(fd, buff, _stream->gcount(), 0);
	setState(_stream->eof() ? RES_DONE : _state.raw());
}
// =================================================================== Builder
//
// Fluent setup. Each setter mutates the wrapped Response directly; `apply()`
// emits the standard header bundle (Server/Date/Connection/Keep-Alive/
// Accept-Ranges) and leaves the response ready for send().

Response::Builder Response::build() {
	return Builder(*this);
}

Response::Builder::Builder(Response &target) : _r(target) {}

Response::Builder &Response::Builder::status(int code) {
	_r._statusCode = (short)code;
	return *this;
}

Response::Builder &Response::Builder::keepAlive(bool keep) {
	_r._keepAlive = keep;
	return *this;
}

Response::Builder &Response::Builder::contentType(const string &value) {
	_r._headers.add("Content-Type", value);
	return *this;
}

Response::Builder &Response::Builder::header(const string &name, const string &value) {
	_r._headers.add(name, value);
	return *this;
}

Response::Builder &Response::Builder::body(iostream *stream) {
	_r._stream.reset(stream);
	return *this;
}

// Each `asXxx` swaps in the matching sender strategy. unique_ptr's reset
// drops the previous sender (if any) — safe because the Builder only runs
// during setup, before send() consults the sender.
#define INSTALL_SENDER(SenderType, TypeTag)               \
	do {                                                  \
		_r._sender.reset(new SenderType());               \
		_r._type = TypeTag;                               \
	} while (0)

Response::Builder &Response::Builder::asLengthed() { INSTALL_SENDER(LengthedSender, LENGTHED_RES); return *this; }
Response::Builder &Response::Builder::asChunked()  { INSTALL_SENDER(ChunkedSender,  CHUNKED_RES);  return *this; }
Response::Builder &Response::Builder::asRanged()   { INSTALL_SENDER(RangedSender,   RANGED_RES);   return *this; }
Response::Builder &Response::Builder::asCGI()      { INSTALL_SENDER(CGISender,      CGI_RES);      return *this; }
Response::Builder &Response::Builder::asUpload()   { INSTALL_SENDER(UploadSender,   UPLOAD_RES);   return *this; }

#undef INSTALL_SENDER

Response::Builder &Response::Builder::cgi(int childFd, Request *req) {
	asCGI();
	_r._cgiFd  = childFd;
	_r._req = req;
	return status(OK).keepAlive(true);
}

Response::Builder &Response::Builder::upload(LocationContext<Type> *location, Request *req) {
	asUpload();
	_r._location = location;
	_r._req      = req;
	return status(CREATED).keepAlive(true);
}

void Response::Builder::apply() {
	_r.init();
	_r.setState(RES_HEADER);
}
