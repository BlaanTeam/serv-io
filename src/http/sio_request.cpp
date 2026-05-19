#include "sio_request.hpp"

#include <cstdlib>
#include <cmath>
#include <cstring>

Request::Request()
	: _state(REQ_INIT),
	  _statusCode(BAD_REQUEST),
	  _method(UNKNOWN) {}

// Request embeds a Body which holds self-referential state. Copies are
// intentionally no-ops so destination slots get a fresh, default-state
// Request that will be populated by the next request cycle.
Request::Request(const Request &copy)
	: _state(REQ_INIT),
	  _statusCode(BAD_REQUEST),
	  _method(UNKNOWN) {
	(void)copy;
}

Request &Request::operator=(const Request &rhs) {
	(void)rhs;
	return *this;
}

Request::~Request() {}

// --------------------------------------------------------- state mutators --

void Request::changeState(short state) {
	_state = state;
}

void Request::fail(short statusCode) {
	_statusCode = statusCode;
	_state = REQ_INVALID;
}

// --------------------------------------------------------- main feed loop --

size_t Request::consume(const char *buf, size_t len) {
	if (_state & (REQ_DONE | REQ_INVALID))
		return 0;

	size_t i = 0;
	while (i < len) {
		// REQ_BODY delegates straight to the Body parser, which knows how to
		// consume large contiguous spans without going through this byte loop.
		if (_state & REQ_BODY) {
			size_t used = _body.consume(buf + i, len - i);
			i += used;
			if (_body.getState() & BODY_ERROR) {
				fail(BAD_REQUEST);
				return i;
			}
			if (_body.getState() & BODY_DONE)
				changeState(REQ_DONE);
			break;  // body parser owns the rest of this chunk
		}

		char c = buf[i++];

		if (_state & REQ_INIT) {
			if (c == '\r' || c == '\n') continue;  // tolerate leading CRLFs
			_line.clear();
			_line += c;
			changeState(REQ_LINE);
			continue;
		}

		if (_line.size() >= REQ_MAX_HEADER_LINE) {
			fail(_state & REQ_LINE ? REQUEST_URI_TOO_LONG : BAD_REQUEST);
			return i;
		}
		_line += c;

		// Lines terminate on LF; CRLF is supported because the trailing CR was
		// included in _line — strip it before parsing.
		if (c == '\n') {
			size_t end = _line.size();
			if (end >= 2 && _line[end - 2] == '\r') {
				_line.erase(end - 2);
			} else {
				_line.erase(end - 1);
			}

			if (_state & REQ_LINE) {
				parseRequestLine();
				_line.clear();
				if (_state & REQ_INVALID) return i;
			} else if (_state & REQ_HEADER) {
				if (_line.empty()) {
					onHeadersComplete();
					_line.clear();
					if (_state & (REQ_INVALID | REQ_DONE | REQ_BODY)) {
						if (_state & REQ_INVALID) return i;
						continue;
					}
				} else {
					parseHeaderLine();
					_line.clear();
					if (_state & REQ_INVALID) return i;
				}
			}
		}
	}
	return i;
}

// --------------------------------------------------------- line handlers --

// Splits the request line on the two single-spaces required by RFC 9112:
//   METHOD SP REQUEST-URI SP HTTP-VERSION
void Request::parseRequestLine() {
	size_t sp1 = _line.find(' ');
	if (sp1 == string::npos) return fail(BAD_REQUEST);
	size_t sp2 = _line.find(' ', sp1 + 1);
	if (sp2 == string::npos) return fail(BAD_REQUEST);

	string method = _line.substr(0, sp1);
	string uri = _line.substr(sp1 + 1, sp2 - sp1 - 1);
	string version = _line.substr(sp2 + 1);

	if (uri.size() > REQ_MAX_URI) return fail(REQUEST_URI_TOO_LONG);
	if (version != HTTP_VERSION) return fail(BAD_REQUEST);
	if (method.empty() || !every(method, ::isupper)) return fail(BAD_REQUEST);

	size_t q = uri.find('?');
	string path = (q == string::npos) ? uri : uri.substr(0, q);
	if (q != string::npos) _query = uri.substr(q + 1);

	pair<bool, string> normalized = normpath(path);
	if (!normalized.first) return fail(BAD_REQUEST);
	_path = normalized.second;

	int idx = 0;
	while (idx < httpMethodCount && httpMethods[idx] != method) ++idx;
	if (idx >= httpMethodCount) return fail(BAD_REQUEST);
	_method = (HttpMethod)(1 << idx);

	changeState(REQ_HEADER);
}

void Request::parseHeaderLine() {
	size_t colon = _line.find(':');
	if (colon == string::npos || colon == 0) return fail(BAD_REQUEST);

	string key = _line.substr(0, colon);
	string value = _line.substr(colon + 1);
	trim(key);
	trim(value);
	_headers.add(key, value);
}

void Request::onHeadersComplete() {
	_body.chooseState(_headers);
	_body.openFile();
	if (_method & (GET | TRACE | OPTIONS | HEAD)) {
		changeState(REQ_DONE);
		return;
	}
	if (_body.getState() & BODY_DONE) {
		changeState(REQ_DONE);
		return;
	}
	changeState(REQ_BODY);
}

// ------------------------------------------------------------ getters etc --

string Request::getPath(void) const { return _path; }
string Request::getQuery(void) const { return _query; }
short  Request::getState(void) const { return _state; }
int    Request::getStatusCode() const { return _statusCode; }
int    Request::getFileno() const { return _body.getFileno(); }
HttpMethod Request::getMethod(void) const { return _method; }

Header             &Request::getHeaders(void) { return _headers; }
map<int, BodyFile> &Request::getBodyFiles() { return _body.getBodyFiles(); }

bool Request::valid() const { return !(_state & REQ_INVALID); }

bool Request::isTooLarge(const int &clientMaxSize) {
	string value = _headers.get("Content-Length");
	if (value.empty()) return false;
	if (!every(value, ::isdigit)) return false;
	return atoll(value.c_str()) > clientMaxSize;
}

bool Request::match(const int &state) const { return _state & state; }

Range Request::getRange() {
	string value = _headers.get("Range");
	if (value.empty()) return Range();
	return Range(value);
}

void Request::reset(void) {
	_state = REQ_INIT;
	_method = UNKNOWN;
	_statusCode = BAD_REQUEST;
	_path.clear();
	_query.clear();
	_line.clear();
	_headers.clear();
	_body.reset();
}

void Request::closeBodyFile() {
	if (match(REQ_DONE)) _body.closeFile();
}
