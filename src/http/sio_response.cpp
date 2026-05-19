#include "sio_response.hpp"

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

// Portable sendfile shim. The kernel ABI differs between platforms; the
// wrapper presents a uniform "send up to `count` bytes from offset, advance
// offset, return bytes-sent or -1 on hard error" contract.
static ssize_t sio_sendfile(int sock, int filefd, off_t *offset, size_t count) {
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
	_lengthState = INIT_LENGTH;
	_stream = nullptr;
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
	_stream = nullptr;
	_fileFd = -1;
	_filePos = 0;
	_fileLen = 0;
	_type = LENGTHED_RES;
	_isCustomStatusCode = false;
	setState(RES_INIT);
}

Response::Response(const Response &copy) {
	_stream = nullptr;
	_fileFd = -1;
	_filePos = 0;
	_fileLen = 0;
	*this = copy;
}

Response &Response::operator=(const Response &rhs) {
	if (this != &rhs) {
		_keepAlive = rhs._keepAlive;
		_stream = rhs._stream;
		_state = rhs._state;
		_type = rhs._type;
		_headers = rhs._headers;
		_isCustomStatusCode = rhs._isCustomStatusCode;
	}
	return *this;
}

Response::~Response() {
	delete _stream;
	if (_fileFd >= 0) close(_fileFd);
}

void Response::init() {
	// !INFO: for security reason we should hide this header in some cases !
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
	_ss << HTTP_VERSION << " " << to_string(_statusCode) << " " << _statusStringCode << CRLF;

	Header::iterator it = _headers.begin();

	while (it != _headers.end()) {
		for (set<string>::iterator it_ = it->second.begin(); it_ != it->second.end(); it_++)
			_ss << it->first << ": " << *it_ << CRLF;

		it++;
	}
}

// Setters

void Response::setStatusCode(const short &statusCode) {
	_statusCode = statusCode;
}

void Response::setState(const int &state) {
	_state = state;
}

void Response::setStream(iostream *stream) {
	_stream = stream;
}

void Response::setConnectionStatus(bool keepAlive) {
	_keepAlive = keepAlive;
}

bool Response::keepAlive(void) const {
	return _keepAlive;
}

void Response::addHeader(const string &name, const string &value) {
	if (_state & (RES_DONE | RES_BODY))
		return;
	// _headers[name] = value;
	_headers.add(name, value);
	setState(RES_HEADER);
}
void Response::send(const sockfd &fd) {
	if (_state & (RES_INIT | RES_HEADER)) {
		if (_type & LENGTHED_RES)
			setupLengthedBody();
		else if (_type & CHUNKED_RES)
			setupChunkedBody();
		else if (_type & RANGED_RES)
			setupRangedBody();
		else if (_type & CGI_RES) {
			if (!_req->match(REQ_DONE) || !setupCGIBody())
				return;
		} else if (_type & UPLOAD_RES) {
			if (!setupUploadBody())  // ? this function will return true in case of the REQ_DONE
				return;
		}
		prepare();
		::send(fd, _ss.str().c_str(), _ss.str().size(), 0);
		::send(fd, CRLF, 2, 0);
		setState(RES_BODY);
	}

	// if ((_statusCode / 100) == 1 || _statusCode == 204 || _statusCode == 301)
	// 	return setState(RES_DONE);

	if (_state & RES_BODY)
		switch (_type) {
		case LENGTHED_RES:
			sendLengthedBody(fd);
			break;
		case CHUNKED_RES:
			sendChunkedBody(fd);
			break;
		case RANGED_RES:
			sendRangedBody(fd);
			break;
		case CGI_RES:
			sendCGIBody(fd);
			break;
		case UPLOAD_RES:
			sendUploadBody(fd);
			break;
		}
}

void Response::setupErrorResponse(const int &statusCode, MainContext<Type> *ctx, bool isBuiltIn) {
	_type = LENGTHED_RES;
	setStatusCode(statusCode);
	setConnectionStatus(false);
	init();
	addHeader("Content-Type", mimeTypes["html"]);

	if (ctx) {
		ErrorPage *errPage = ctx->getErrorPage(statusCode);
		if (errPage) {
			if (!errPage->exists() && isBuiltIn)
				setupErrorResponse(NOT_FOUND, ctx, false);
			else if (!errPage->exists()) {
				if (errno == EACCES && !isBuiltIn)
					return setupErrorResponse(FORBIDDEN, ctx, true);
				setStream(buildResponseBody(NOT_FOUND));
			} else {
				addHeader("Content-Type", mimeTypes.choiceMimeType(errPage->page));
				setStream(new fstream(errPage->page, ios::in));
			}
			return;
		}
	}
	setStream(buildResponseBody(statusCode));
}

void Response::setupRedirectResponse(Redirect *redir, MainContext<Type> *ctx) {
	_type = LENGTHED_RES;
	redir->prepare(ctx);

	setStatusCode(redir->code);
	init();
	addHeader("Content-Type", mimeTypes["html"]);

	if (redir->isRedirect) {
		addHeader("Location", redir->path);
		setStream(buildResponseBody(redir->code));
		return;
	}
	addHeader("Content-Type", mimeTypes[""]);
	iostream *ss = new stringstream;
	(*ss) << redir->path;
	setStream(ss);
}

void Response::setupDirectoryListing(const string &path, const string &title) {
	_type = LENGTHED_RES;
	setStatusCode(200);
	setConnectionStatus(true);
	init();
	addHeader("Content-Type", mimeTypes["html"]);
	setStream(buildDirectoryListing(path, title));
}

bool Response::setupNormalResponse(const string &path) {
	int fd = ::open(path.c_str(), O_RDONLY);
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
	setStatusCode(200);
	setConnectionStatus(true);
	init();
	addHeader("Content-Type", mimeTypes.choiceMimeType(path));
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
	_state = state;
}

void Response::setupCGIResponse(const int &fd, Request *req) {
	_type = CGI_RES;
	_fd = fd;
	_req = req;

	setStatusCode(OK);
	setConnectionStatus(true);
}

bool Response::match(const int &state) const {
	return _state & state;
}

void Response::reset(void) {
	// clear the headers
	_headers.clear();

	// clear the stringstream
	_ss.str("");
	_ss.clear();

	_lengthState = INIT_LENGTH;

	delete _stream;
	_stream = nullptr;
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
		ssize_t sent = sio_sendfile(fd, _fileFd, &_filePos, want);
		if (sent < 0 || _filePos >= _fileLen)
			setState(RES_DONE);
		return;
	}
	if (!_stream)
		return;
	char buff[(1 << 10)];

	_stream->read(buff, (1 << 10));
	::send(fd, buff, _stream->gcount(), 0);
	setState(_stream->eof() ? RES_DONE : _state);
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
		_ss << hex << _stream->gcount() << CRLF;
		::send(fd, _ss.str().c_str(), _ss.str().size(), 0);
		::send(fd, buff, _stream->gcount(), 0);
		::send(fd, CRLF, 2, 0);
		_ss.str("");
		_ss.clear();
	}
	setState(_stream->eof() ? RES_DONE : _state);
}

void Response::setupRangedBody() {
	RangeSpecifier range = _range.getRangeSpecifiers()[0];
	const size_t   fileSize = (_fileFd >= 0) ? (size_t)_fileLen : getFileSize(_stream);

	if (_fileFd >= 0)
		addHeader("Content-Length", to_string((long long)range.getContentLength(fileSize)));
	else
		addHeader("Content-Length", to_string(range.getContentLength(_stream)));
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
	RangeSpecifier range = _range.getRangeSpecifiers()[0];
	const size_t   fileSize = (_fileFd >= 0) ? (size_t)_fileLen : getFileSize(_stream);

	range.rangeEnd = min(range.rangeEnd, fileSize);

	if (_lengthState == INIT_LENGTH) {
		if (_fileFd >= 0) {
			_filePos = (off_t)range.rangeStart;
			_length = range.getContentLength(fileSize);
		} else {
			range.setupSeek(_stream);
			_length = range.getContentLength(_stream);
		}
		_lengthState = ONGOING_LENGTH;
	}

	if (_fileFd >= 0) {
		if (_length <= 0) {
			_lengthState = DONE_LENGTH;
		} else {
			size_t  want = (_length > (1 << 16)) ? (size_t)(1 << 16) : (size_t)_length;
			ssize_t sent = sio_sendfile(fd, _fileFd, &_filePos, want);
			if (sent < 0) {
				_lengthState = DONE_LENGTH;
			} else {
				_length -= (int)sent;
				if (_length <= 0) _lengthState = DONE_LENGTH;
			}
		}
	} else if (_length > (1 << 10))
		sendKiloByte(fd);
	else
		sendLessThanKiloByte(fd);

	if (_lengthState & DONE_LENGTH)
		_state = RES_DONE;
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
	_lengthState = DONE_LENGTH;
}

bool Response::setupCGIBody() {
	string line = "";
	int    nbyte;
	char   chr;
	bool   got = false;
	while ((nbyte = read(_fd, &chr, 1)) > 0) {
		line += chr;
		if (line.find(CRLF) != string::npos || line.find(LF) != string::npos) {
			stringstream ss(line);
			parseHeaders(ss);
			line = "";
			if (_state & RES_BODY)
				break;
		}
		got = true;
	}
	if (nbyte <= 0 && !got)
		return false;
	changeState(RES_HEADER);  // reset the state to header!
	string contentType = _headers.get("Content-Type");
	if (contentType.empty())
		contentType = mimeTypes[""];

	string status = _headers.get("Status");
	if (!status.empty()) {
		char buff[(1 << 10)];
		sscanf(status.c_str(), "%hd %[^\n]1000s", &_statusCode, buff);
		_isCustomStatusCode = true;
		_statusStringCode = buff;
		// ? INFO: we can remove Status from headers cuz it useless !!
	}
	addHeader("Content-Type", contentType);
	init();
	return true;
}

void Response::sendCGIBody(const sockfd &fd) {
	char buff[(1 << 10)];
	int  nbyte = read(_fd, buff, (1 << 10));
	if (nbyte <= 0) { // TODO: check content-length also
		setState(RES_DONE);
		close(_fd);
	} else
		::send(fd, buff, nbyte, 0);
}

void Response::extractRange(Request &req) {
	_type = LENGTHED_RES;
	_range = req.getRange();
	if (_range.valid())
		_type = RANGED_RES;
}

void Response::setupUploadResponse(LocationContext<Type> *location, Request *req) {
	_type = UPLOAD_RES;
	_location = location;
	_req = req;
	setStatusCode(CREATED);
	setConnectionStatus(true);
	changeState(RES_HEADER);
}

bool Response::setupUploadBody() {
	if (!_req->match(REQ_DONE))
		return false;

	// INFO: we can return interanl error in case of error(lazy)!

	map<int, BodyFile> &bodyFiles = _req->getBodyFiles();
	map<string, bool>   fileStatus;

	string filename;
	{
		map<int, BodyFile>::iterator it = bodyFiles.begin();
		while (it != bodyFiles.end()) {
			string oldPath = it->second.getFilename();
			filename = it->second.extractFilename();
			if (filename.empty())
				filename = oldPath;
			string newPath = joinPath(_location->getUploadStore(), filename);
			bool   failed = false;
			if (!rename(oldPath.c_str(), newPath.c_str()))
				failed = true;
			fileStatus[filename] = failed;
			it++;
		}
	}
	_stream = new stringstream;
	_stream->write("<h1>Upload Succeffuly</h1>", 20);
	map<string, bool>::iterator it = fileStatus.begin();
	while (it != fileStatus.end()) {
		(*_stream) << "<br/><span>" << it->first << " -- " << (it->second ? "Uploaded" : "Not Uploaded !") << "</span>" << endl;
		it++;
	}

	addHeader("Content-Length", to_string(getFileSize(_stream)));

	return true;
}

void Response::sendUploadBody(const sockfd &fd) {
	if (!_stream)
		return;
	char buff[(1 << 10)];

	_stream->read(buff, (1 << 10));
	::send(fd, buff, _stream->gcount(), 0);
	setState(_stream->eof() ? RES_DONE : _state);
}