#include "body.hpp"

#include <cctype>
#include <cstdlib>
#include <sstream>

#include "body_parser.hpp"
#include "boundary.hpp"
#include "utility/helpers.hpp"
#include "utility/utils.hpp"

using namespace std;

// ---------------------------------------------------------------- BodyFile --

BodyFile::BodyFile() : _file(NULL) {}

BodyFile::~BodyFile() {
	if (_file) {
		fclose(_file);
		_file = NULL;
	}
}

void BodyFile::adoptFile(FILE *file, const string &filename) {
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

FILE  *BodyFile::file()       { return _file; }
string BodyFile::tmpPath() const { return _filename; }

string BodyFile::clientFilename() {
	const servio::Option<string> header = _headers.get("Content-Disposition");
	if (header.isNone()) return "";

	string value = header.unwrap();
	const size_t pos = value.find("filename=");
	if (pos == string::npos) return "";

	value = value.substr(pos + 9);
	const size_t semi = value.find(';');
	if (semi != string::npos) value = value.substr(0, semi);
	trim(value, " \"");
	return value;
}

// -------------------------------------------------------------------- Body --

Body::Body()
	: _bodyFile(NULL), _strategyChosen(false), _noBody(false) {}

// Self-referential state (the MultipartBodyParser holds a reference to
// _bodyFiles) would dangle on default copy, so copies install a fresh,
// default-state Body — the next request cycle will repopulate it.
Body::Body(const Body &copy)
	: _bodyFile(NULL), _strategyChosen(false), _noBody(false) {
	(void)copy;
}

Body &Body::operator=(const Body &rhs) {
	(void)rhs;
	return *this;
}

Body::~Body() {
	closeFile();
	// _parser cleaned up automatically by unique_ptr.
}

void Body::openTmpFile() {
	if (_bodyFile) return;
	_bodyFilePath = "/tmp/.servio_" + to_string(getmstime()) + "_body.io";
	_bodyFile = fopen(_bodyFilePath.c_str(), "w+");
}

void Body::closeFile() {
	if (_bodyFile) {
		fclose(_bodyFile);
		_bodyFile = NULL;
	}
}

// --------------------------------------------------------- factory method --

// Transfer-Encoding: chunked takes precedence over Content-Length.
static bool isChunkedEncoding(const string &value) {
	stringstream ss(value);
	string       token;
	while (getline(ss, token, ',')) {
		trim(token);
		if (iequalString(token, "Chunked"))
			return true;
	}
	return false;
}

static servio::Option<size_t> parseContentLength(const string &raw) {
	string trimmed = raw;
	trim(trimmed);
	if (trimmed.empty() || trimmed[0] < '1' || trimmed[0] > '9') return servio::None<size_t>();
	if (!every(trimmed, ::isdigit)) return servio::None<size_t>();
	const size_t length = (size_t)atoll(trimmed.c_str());
	return length > 0 ? servio::Some(length) : servio::None<size_t>();
}

void Body::chooseStrategy(Header &headers) {
	if (_strategyChosen) return;
	_strategyChosen = true;

	openTmpFile();

	const servio::Option<string> te = headers.get("Transfer-Encoding");
	if (te.isSome() && isChunkedEncoding(te.unwrap())) {
		_parser.reset(new ChunkedBodyParser(_bodyFile));
		return;
	}

	const servio::Option<string> ct = headers.get("Content-Type");
	if (ct.isSome()) {
		const servio::Result<Boundary, string> boundary = Boundary::parse(ct.unwrap());
		if (boundary.isOk()) {
			_parser.reset(new MultipartBodyParser(boundary.unwrap(), _bodyFiles));
			return;
		}
	}

	const servio::Option<size_t> cl =
	    headers.get("Content-Length").isSome()
	        ? parseContentLength(headers.get("Content-Length").unwrap())
	        : servio::None<size_t>();
	if (cl.isSome()) {
		_parser.reset(new LengthedBodyParser(_bodyFile, cl.unwrap()));
		return;
	}

	// No transfer-encoding, no boundary, no positive Content-Length.
	_noBody = true;
}

// ------------------------------------------------------- delegation API ----

size_t Body::consume(const char *buf, size_t len) {
	if (!_parser || _noBody || isDone() || isError() || !len)
		return 0;
	return _parser->consume(buf, len);
}

bool Body::isDone() const {
	if (_noBody) return true;
	return _parser && _parser->isDone();
}

bool Body::isError() const {
	return _parser && _parser->isError();
}

int Body::fileno() const {
	return _bodyFile ? ::fileno(_bodyFile) : -1;
}

map<int, BodyFile> &Body::bodyFiles() {
	return _bodyFiles;
}

void Body::reset() {
	_parser.reset();
	closeFile();
	_bodyFilePath.clear();
	_bodyFiles.clear();   // BodyFile dtors close each part file
	_strategyChosen = false;
	_noBody = false;
}
