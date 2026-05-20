#include "body.hpp"

#include <cctype>
#include <cstdlib>
#include <sstream>

#include "body_parser.hpp"
#include "boundary.hpp"
#include "utility/helpers.hpp"
#include "utility/utils.hpp"

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
	string value = _headers.get("Content-Disposition");
	if (value.empty()) return value;

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
	: _parser(NULL), _bodyFile(NULL), _strategyChosen(false), _noBody(false) {}

// Self-referential state (the MultipartBodyParser holds a reference to
// _bodyFiles) would dangle on default copy, so copies install a fresh,
// default-state Body — the next request cycle will repopulate it.
Body::Body(const Body &copy)
	: _parser(NULL), _bodyFile(NULL), _strategyChosen(false), _noBody(false) {
	(void)copy;
}

Body &Body::operator=(const Body &rhs) {
	(void)rhs;
	return *this;
}

Body::~Body() {
	destroyParser();
	closeFile();
}

void Body::destroyParser() {
	delete _parser;
	_parser = NULL;
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

void Body::chooseStrategy(Header &headers) {
	if (_strategyChosen) return;
	_strategyChosen = true;

	openTmpFile();

	// Transfer-Encoding: chunked takes precedence over Content-Length.
	const string te = headers.get("Transfer-Encoding");
	if (!te.empty()) {
		stringstream ss(te);
		string       token;
		while (getline(ss, token, ',')) {
			trim(token);
			if (iequalString(token, "Chunked")) {
				_parser = new ChunkedBodyParser(_bodyFile);
				return;
			}
		}
	}

	const servio::Result<Boundary, string> boundary =
		Boundary::parse(headers.get("Content-Type"));
	if (boundary.isOk()) {
		_parser = new MultipartBodyParser(boundary.unwrap(), _bodyFiles);
		return;
	}

	const string cl = headers.get("Content-Length");
	if (!cl.empty()) {
		string trimmed = cl;
		trim(trimmed);
		if (trimmed.length() && trimmed[0] >= '1' && trimmed[0] <= '9' && every(trimmed, ::isdigit)) {
			const size_t length = (size_t)atoll(trimmed.c_str());
			if (length > 0) {
				_parser = new LengthedBodyParser(_bodyFile, length);
				return;
			}
		}
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
	destroyParser();
	closeFile();
	_bodyFilePath.clear();
	_bodyFiles.clear();   // BodyFile dtors close each part file
	_strategyChosen = false;
	_noBody = false;
}
