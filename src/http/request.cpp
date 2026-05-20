#include "request.hpp"

#include <cstdlib>
#include <cmath>
#include <cstring>

using namespace std;

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
	_state.replace(state);
}

void Request::fail(short statusCode) {
	_statusCode = statusCode;
	_state.replace(REQ_INVALID);
}

// --------------------------------------------------------- main feed loop --

// Three parsing regimes:
//   - REQ_INIT/REQ_LINE/REQ_HEADER: bytes flow through `_lineReader`, which
//     hands back complete lines for `parseRequestLine` / `parseHeaderLine`.
//   - REQ_BODY: bytes flow straight into `_body.consume` (the body parser
//     owns whatever recv chunk remains).
//   - REQ_DONE / REQ_INVALID: stop.
size_t Request::consume(const char *buf, size_t len) {
	if (_state.any(REQ_DONE | REQ_INVALID))
		return 0;

	size_t i = 0;
	while (i < len) {
		if (_state.any(REQ_BODY)) {
			const size_t used = _body.consume(buf + i, len - i);
			i += used;
			if (_body.isError()) {
				fail(BAD_REQUEST);
				return i;
			}
			if (_body.isDone())
				changeState(REQ_DONE);
			break;  // body parser owns the rest of this chunk
		}

		// Feed bytes to the line scanner; it stops after the first complete
		// line so we can react to state transitions between lines (in
		// particular, the empty header line that hands the rest of the
		// recv buffer to the body parser).
		const size_t taken = _lineReader.feed(buf + i, len - i);
		i += taken;

		if (_lineReader.pendingSize() >= REQ_MAX_HEADER_LINE) {
			fail(_state.any(REQ_LINE) ? REQUEST_URI_TOO_LONG : BAD_REQUEST);
			return i;
		}

		const servio::Option<string> next = _lineReader.takeLine();
		if (next.isNone()) {
			if (taken == 0) break;   // need more bytes from the next recv
			continue;
		}

		const string line = next.unwrap();

		if (_state.any(REQ_INIT)) {
			if (line.empty()) continue;   // tolerate leading blank lines
			changeState(REQ_LINE);
		}

		if (_state.any(REQ_LINE)) {
			parseRequestLine(line);
			if (_state.any(REQ_INVALID)) return i;
			continue;
		}

		if (_state.any(REQ_HEADER)) {
			if (line.empty()) {
				onHeadersComplete();
				if (_state.any(REQ_INVALID)) return i;
				continue;  // outer loop now dispatches to REQ_BODY branch
			}
			parseHeaderLine(line);
			if (_state.any(REQ_INVALID)) return i;
		}
	}
	return i;
}

// --------------------------------------------------------- line handlers --

// Splits the request line on the two single-spaces required by RFC 9112:
//   METHOD SP REQUEST-URI SP HTTP-VERSION
void Request::parseRequestLine(const string &line) {
	const size_t sp1 = line.find(' ');
	if (sp1 == string::npos) return fail(BAD_REQUEST);
	const size_t sp2 = line.find(' ', sp1 + 1);
	if (sp2 == string::npos) return fail(BAD_REQUEST);

	const string method  = line.substr(0, sp1);
	const string uri     = line.substr(sp1 + 1, sp2 - sp1 - 1);
	const string version = line.substr(sp2 + 1);

	if (uri.size() > REQ_MAX_URI) return fail(REQUEST_URI_TOO_LONG);
	if (version != HTTP_VERSION) return fail(BAD_REQUEST);
	if (method.empty() || !every(method, ::isupper)) return fail(BAD_REQUEST);

	const size_t q = uri.find('?');
	const string path = (q == string::npos) ? uri : uri.substr(0, q);
	if (q != string::npos) _query = uri.substr(q + 1);

	// Rust-style fold: assign on Some, mark failure on None.
	bool pathInvalid = false;
	normpath(path).match(
	    [&](const string &normalized) { _path = normalized; },
	    [&] { pathInvalid = true; });
	if (pathInvalid) return fail(BAD_REQUEST);

	int idx = 0;
	while (idx < httpMethodCount && httpMethods[idx] != method) ++idx;
	if (idx >= httpMethodCount) return fail(BAD_REQUEST);
	_method = (HttpMethod)(1 << idx);

	changeState(REQ_HEADER);
}

void Request::parseHeaderLine(const string &line) {
	const size_t colon = line.find(':');
	if (colon == string::npos || colon == 0) return fail(BAD_REQUEST);

	string key   = line.substr(0, colon);
	string value = line.substr(colon + 1);
	trim(key);
	trim(value);
	_headers.add(key, value);
}

void Request::onHeadersComplete() {
	_body.chooseStrategy(_headers);
	const bool bodylessMethod = (_method & (GET | TRACE | OPTIONS | HEAD)) != 0;
	if (bodylessMethod || _body.isDone()) {
		changeState(REQ_DONE);
		return;
	}
	changeState(REQ_BODY);
}

// ------------------------------------------------------------ getters etc --

string Request::path(void) const { return _path; }
string Request::query(void) const { return _query; }
short  Request::state(void) const { return _state.raw(); }
int    Request::statusCode() const { return _statusCode; }
int    Request::fileno() const { return _body.fileno(); }
HttpMethod Request::method(void) const { return _method; }

Header             &Request::headers(void) { return _headers; }
map<int, BodyFile> &Request::bodyFiles() { return _body.bodyFiles(); }

bool Request::valid() const { return !_state.any(REQ_INVALID); }

bool Request::isTooLarge(const int &clientMaxSize) {
	const servio::Option<string> value = _headers.get("Content-Length");
	if (value.isNone()) return false;
	const string &v = value.unwrap();
	if (!every(v, ::isdigit)) return false;
	return atoll(v.c_str()) > clientMaxSize;
}

bool Request::match(const int &state) const { return _state.any(state); }

Range Request::range() {
	const servio::Option<string> value = _headers.get("Range");
	if (value.isNone()) return Range();
	return Range::parse(value.unwrap()).unwrapOr(Range());
}

void Request::reset(void) {
	_state.replace(REQ_INIT);
	_method = UNKNOWN;
	_statusCode = BAD_REQUEST;
	_path.clear();
	_query.clear();
	_lineReader.reset();
	_headers.clear();
	_body.reset();
}

void Request::closeBodyFile() {
	if (match(REQ_DONE)) _body.closeFile();
}
