#include "body.hpp"

#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------- BodyFile --

BodyFile::BodyFile() : _file(nullptr) {}

BodyFile::~BodyFile() {
	if (_file) {
		fclose(_file);
		_file = nullptr;
	}
}

void BodyFile::addFile(FILE *file, const string &filename) {
	_file = file;
	_filename = filename;
}

void BodyFile::addHeader(const string &key, const string &value) {
	_headers.add(key, value);
}

void BodyFile::write(const char *data, size_t len) {
	if (!_file || !len) return;
	fwrite(data, 1, len, _file);
}

FILE *BodyFile::getFile() {
	return _file;
}

string BodyFile::getFilename() const {
	return _filename;
}

string BodyFile::extractFilename() {
	string value = _headers.get("Content-Disposition");
	if (value.empty())
		return value;
	size_t idx = value.find("filename=");
	if (idx == string::npos)
		return "";
	value = value.substr(idx + 9);
	size_t semi = value.find(';');
	if (semi != string::npos)
		value = value.substr(0, semi);
	trim(value, " \"");
	return value;
}

// -------------------------------------------------------------------- Body --

Body::Body()
	: _bodyFile(nullptr),
	  _bodyState(BODY_INIT),
	  _contentLength(0),
	  _written(0),
	  _chunkPhase(CHUNK_SIZE_LINE),
	  _chunkSize(0),
	  _chunkRemaining(0),
	  _mpPhase(MP_PREAMBLE),
	  _fileIndex(0),
	  _partFileOpen(false) {}

// Body holds self-referential state (a StreamSearch that points into this
// instance). Default copies would dangle, so copy operations are no-ops:
// container slots end up holding fresh, default-constructed Bodies, and the
// real state is populated later during request parsing.
Body::Body(const Body &copy)
	: StreamSearch::Sink(copy),
	  _bodyFile(nullptr),
	  _bodyState(BODY_INIT),
	  _contentLength(0),
	  _written(0),
	  _chunkPhase(CHUNK_SIZE_LINE),
	  _chunkSize(0),
	  _chunkRemaining(0),
	  _mpPhase(MP_PREAMBLE),
	  _fileIndex(0),
	  _partFileOpen(false) {
	(void)copy;
}

Body &Body::operator=(const Body &rhs) {
	(void)rhs;
	return *this;
}

Body::~Body() {
	closeFile();
}

short Body::getState() const {
	return _bodyState;
}

void Body::setState(int state) {
	_bodyState = state;
}

map<int, BodyFile> &Body::getBodyFiles() {
	return _bodyFiles;
}

int Body::getFileno() const {
	return _bodyFile ? fileno(_bodyFile) : -1;
}

void Body::chooseState(Header &headers) {
	string te = headers.get("Transfer-Encoding");
	if (!te.empty()) {
		stringstream ss(te);
		string       part;
		while (getline(ss, part, ',')) {
			trim(part);
			if (iequalString(part, "Chunked"))
				return setState(BODY_OPEN | CHUNKED_BODY);
		}
	}
	string ct = headers.get("Content-Type");
	_boundary = Boundary(ct);
	if (_boundary.valid()) {
		string needle = "\r\n--" + _boundary.value();
		_search.init(needle, this);
		// First boundary in the body lacks a leading CRLF — pre-seed the
		// search with "\r\n" so the parser uniformly treats every boundary as
		// "\r\n--<boundary>".
		_search.feed("\r\n", 2);
		return setState(BODY_OPEN | MULTIPARTED_BODY);
	}

	string cl = headers.get("Content-Length");
	if (!cl.empty()) {
		trim(cl);
		if (cl.length() && cl[0] >= '1' && cl[0] <= '9' && every(cl, ::isdigit))
			_contentLength = (size_t)atoll(cl.c_str());
	}
	if (_contentLength > 0)
		setState(BODY_OPEN | LENGTHED_BODY);
	else
		setState(BODY_OPEN | BODY_DONE);
}

void Body::openFile() {
	if (_bodyState & BODY_OPEN) {
		_filename = "/tmp/.servio_" + to_string(getmstime()) + "_body.io";
		_bodyFile = fopen(_filename.c_str(), "w+");
		_bodyState &= ~BODY_OPEN;
	}
}

void Body::closeFile() {
	if (_bodyFile) {
		fclose(_bodyFile);
		_bodyFile = nullptr;
	}
}

size_t Body::consume(const char *buf, size_t len) {
	if (!(_bodyState & BODY_READ) || (_bodyState & BODY_DONE) || !len)
		return 0;

	if (_bodyState & LENGTHED_BODY)
		return consumeLengthed(buf, len);
	if (_bodyState & CHUNKED_BODY)
		return consumeChunked(buf, len);
	if (_bodyState & MULTIPARTED_BODY)
		return consumeMultipart(buf, len);
	return 0;
}

size_t Body::consumeLengthed(const char *buf, size_t len) {
	size_t remaining = (_contentLength > _written) ? (_contentLength - _written) : 0;
	size_t take = (len < remaining) ? len : remaining;
	if (take && _bodyFile)
		fwrite(buf, 1, take, _bodyFile);
	_written += take;
	if (_written >= _contentLength)
		_bodyState |= BODY_DONE;
	return take;
}

// Chunked transfer decoding driven by a tiny state machine. Each call
// consumes as many bytes as possible and bails out on incomplete frames so
// the next recv() can resume.
size_t Body::consumeChunked(const char *buf, size_t len) {
	size_t i = 0;
	while (i < len && !(_bodyState & BODY_DONE) && !(_bodyState & BODY_ERROR)) {
		switch (_chunkPhase) {
		case CHUNK_SIZE_LINE: {
			char c = buf[i++];
			if (c == '\r') continue;  // tolerate CR; LF terminates the line
			if (c == '\n') {
				if (_chunkSizeLine.empty()) {
					_bodyState |= BODY_ERROR;
					return i;
				}
				_chunkSize = (size_t)strtol(_chunkSizeLine.c_str(), nullptr, 16);
				_chunkRemaining = _chunkSize;
				_chunkSizeLine.clear();
				if (_chunkSize == 0) {
					_chunkPhase = CHUNK_TRAILER_CR;
				} else {
					_chunkPhase = CHUNK_DATA;
				}
				continue;
			}
			if (!isxdigit((unsigned char)c)) {
				_bodyState |= BODY_ERROR;
				return i;
			}
			_chunkSizeLine += c;
			break;
		}
		case CHUNK_DATA: {
			size_t avail = len - i;
			size_t take = (avail < _chunkRemaining) ? avail : _chunkRemaining;
			if (take && _bodyFile)
				fwrite(buf + i, 1, take, _bodyFile);
			i += take;
			_chunkRemaining -= take;
			if (_chunkRemaining == 0)
				_chunkPhase = CHUNK_DATA_CR;
			break;
		}
		case CHUNK_DATA_CR: {
			char c = buf[i++];
			if (c == '\r') {
				_chunkPhase = CHUNK_DATA_LF;
			} else if (c == '\n') {
				_chunkPhase = CHUNK_SIZE_LINE;
			} else {
				_bodyState |= BODY_ERROR;
				return i;
			}
			break;
		}
		case CHUNK_DATA_LF: {
			char c = buf[i++];
			if (c != '\n') {
				_bodyState |= BODY_ERROR;
				return i;
			}
			_chunkPhase = CHUNK_SIZE_LINE;
			break;
		}
		case CHUNK_TRAILER_CR: {
			char c = buf[i++];
			if (c == '\r') {
				_chunkPhase = CHUNK_TRAILER_LF;
			} else if (c == '\n') {
				_bodyState |= BODY_DONE;
				return i;
			} else {
				// Trailer headers are not supported — skip to next CRLF.
				_chunkPhase = CHUNK_TRAILER_CR;
			}
			break;
		}
		case CHUNK_TRAILER_LF: {
			char c = buf[i++];
			if (c != '\n') {
				_bodyState |= BODY_ERROR;
				return i;
			}
			_bodyState |= BODY_DONE;
			return i;
		}
		}
	}
	return i;
}

// Multipart parsing driven by the StreamSearch needle scanner.
// We alternate between two regimes:
//  1. control regimes (MP_AFTER_BOUNDARY, MP_HEADERS) consume bytes directly;
//  2. data regimes (MP_PREAMBLE, MP_PART_BODY) push bytes through the
//     StreamSearch, which streams non-needle data to onMultipartData and flips
//     `matched()` when the boundary needle has been observed in full.
size_t Body::consumeMultipart(const char *buf, size_t len) {
	size_t i = 0;
	while (i < len && _mpPhase != MP_EPILOGUE) {
		if (_mpPhase == MP_PREAMBLE || _mpPhase == MP_PART_BODY) {
			size_t taken = _search.feed(buf + i, len - i);
			i += taken;
			if (_search.matched()) {
				if (_mpPhase == MP_PART_BODY)
					closePartFile();
				_search.clearMatch();
				_mpPhase = MP_AFTER_BOUNDARY;
				_afterTail.clear();
			} else {
				break;  // chunk exhausted without finding boundary
			}
			continue;
		}

		if (_mpPhase == MP_AFTER_BOUNDARY) {
			while (i < len && _afterTail.size() < 2)
				_afterTail += buf[i++];
			if (_afterTail.size() < 2)
				break;
			if (_afterTail == "--") {
				_mpPhase = MP_EPILOGUE;
				_bodyState |= BODY_DONE;
				return i;
			}
			if (_afterTail != "\r\n") {
				_bodyState |= BODY_ERROR;
				return i;
			}
			_headerLine.clear();
			_mpPhase = MP_HEADERS;
			continue;
		}

		if (_mpPhase == MP_HEADERS) {
			while (i < len) {
				char c = buf[i++];
				_headerLine += c;
				size_t hl = _headerLine.size();
				if (hl >= 2 && _headerLine[hl - 2] == '\r' && _headerLine[hl - 1] == '\n') {
					if (hl == 2) {
						// Empty line — headers done. Open the part file and
						// switch to body scanning.
						_headerLine.clear();
						openPartFile();
						_mpPhase = MP_PART_BODY;
						break;
					}
					parsePartHeaderLine(_headerLine);
					_headerLine.clear();
				}
			}
			continue;
		}
	}
	return i;
}

void Body::onData(const char *data, size_t len) {
	if (_mpPhase == MP_PREAMBLE) return;  // discard preamble
	if (_mpPhase == MP_PART_BODY && _partFileOpen) {
		map<int, BodyFile>::iterator it = _bodyFiles.find(_fileIndex);
		if (it != _bodyFiles.end())
			it->second.write(data, len);
	}
}

void Body::openPartFile() {
	string filename = "/tmp/.servio_" + to_string(getmstime()) + "_" + to_string(_fileIndex) + "_upload.io";
	FILE  *file = fopen(filename.c_str(), "w+");
	if (!file) {
		_bodyState |= BODY_ERROR;
		return;
	}
	_bodyFiles[_fileIndex].addFile(file, filename);
	_partFileOpen = true;
}

void Body::closePartFile() {
	if (!_partFileOpen) return;
	map<int, BodyFile>::iterator it = _bodyFiles.find(_fileIndex);
	if (it != _bodyFiles.end() && it->second.getFile())
		fflush(it->second.getFile());
	_partFileOpen = false;
	_fileIndex++;
}

void Body::parsePartHeaderLine(const string &line) {
	// line ends with CRLF
	if (line.size() < 2) return;
	size_t end = line.size() - 2;
	size_t colon = line.find(':');
	if (colon == string::npos || colon >= end) return;
	string key = line.substr(0, colon);
	string value = line.substr(colon + 1, end - colon - 1);
	trim(key);
	trim(value);
	_bodyFiles[_fileIndex].addHeader(key, value);
}

void Body::reset() {
	closeFile();

	_bodyState = BODY_INIT;
	_contentLength = 0;
	_written = 0;

	_chunkPhase = CHUNK_SIZE_LINE;
	_chunkSize = 0;
	_chunkRemaining = 0;
	_chunkSizeLine.clear();

	_mpPhase = MP_PREAMBLE;
	_afterTail.clear();
	_headerLine.clear();
	_search.reset();
	_partFileOpen = false;
	_fileIndex = 0;

	for (map<int, BodyFile>::iterator it = _bodyFiles.begin(); it != _bodyFiles.end(); ++it) {
		FILE *f = it->second.getFile();
		if (f) fclose(f);
	}
	_bodyFiles.clear();
}
