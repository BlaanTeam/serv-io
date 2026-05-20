#include "body_parser.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "utility/utils.hpp"

BodyParser::~BodyParser() {}

// -------------------------------------------------------- LengthedBodyParser --

LengthedBodyParser::LengthedBodyParser(FILE *dst, size_t contentLength)
	: _dst(dst), _contentLength(contentLength), _written(0) {}

size_t LengthedBodyParser::consume(const char *buf, size_t len) {
	const size_t remaining = (_contentLength > _written) ? (_contentLength - _written) : 0;
	const size_t take = (len < remaining) ? len : remaining;
	if (take && _dst)
		fwrite(buf, 1, take, _dst);
	_written += take;
	return take;
}

bool LengthedBodyParser::isDone() const {
	return _written >= _contentLength;
}

bool LengthedBodyParser::isError() const {
	return false;
}

// --------------------------------------------------------- ChunkedBodyParser --

ChunkedBodyParser::ChunkedBodyParser(FILE *dst)
	: _dst(dst), _phase(ReadSizeLine), _chunkRemaining(0), _done(false), _error(false) {}

bool ChunkedBodyParser::isDone() const { return _done; }
bool ChunkedBodyParser::isError() const { return _error; }

// Parses a single hex size line. Returns false on bad input.
static bool parseHexSize(const string &line, size_t &out) {
	if (line.empty()) return false;
	char *end = NULL;
	const long n = strtol(line.c_str(), &end, 16);
	if (n < 0 || end == NULL || *end != '\0') return false;
	out = (size_t)n;
	return true;
}

// Consumes as many bytes as possible and bails out on incomplete frames so
// the next recv() can resume. Size and trailer lines flow through
// `_lineReader`; the data span is byte-counted directly.
size_t ChunkedBodyParser::consume(const char *buf, size_t len) {
	size_t i = 0;
	while (i < len && !_done && !_error) {
		switch (_phase) {
		case ReadSizeLine: {
			i += _lineReader.feed(buf + i, len - i);
			servio::Option<string> line = _lineReader.takeLine();
			if (line.isNone()) return i;

			size_t size = 0;
			if (!parseHexSize(line.unwrap(), size)) {
				_error = true;
				return i;
			}
			_chunkRemaining = size;
			_phase = (size == 0) ? ReadTrailerLine : ReadData;
			break;
		}

		case ReadData: {
			const size_t avail = len - i;
			const size_t take  = (avail < _chunkRemaining) ? avail : _chunkRemaining;
			if (take && _dst)
				fwrite(buf + i, 1, take, _dst);
			i += take;
			_chunkRemaining -= take;
			if (_chunkRemaining == 0) {
				_lineReader.reset();   // about to use it for the CRLF terminator
				_phase = ReadDataCRLF;
			}
			break;
		}

		case ReadDataCRLF: {
			i += _lineReader.feed(buf + i, len - i);
			servio::Option<string> line = _lineReader.takeLine();
			if (line.isNone()) return i;
			// The terminator must be an empty line — anything else is a framing error.
			if (!line.unwrap().empty()) {
				_error = true;
				return i;
			}
			_lineReader.reset();
			_phase = ReadSizeLine;
			break;
		}

		case ReadTrailerLine: {
			i += _lineReader.feed(buf + i, len - i);
			servio::Option<string> line = _lineReader.takeLine();
			if (line.isNone()) return i;
			if (line.unwrap().empty()) {
				_done = true;
				return i;
			}
			// Non-empty trailer header — accept and discard.
			break;
		}
		}
	}
	return i;
}

// ------------------------------------------------------- MultipartBodyParser --

MultipartBodyParser::MultipartBodyParser(const Boundary &boundary, map<int, BodyFile> &files)
	: _files(files),
	  _phase(Preamble),
	  _fileIndex(0),
	  _partOpen(false),
	  _done(false),
	  _error(false) {
	_search.init("\r\n--" + boundary.value(), this);
	// Pre-feed "\r\n" so the very first boundary (which has no leading CRLF)
	// is matched by the same needle as later ones.
	_search.feed("\r\n", 2);
}

bool MultipartBodyParser::isDone()  const { return _done; }
bool MultipartBodyParser::isError() const { return _error; }

// Alternates between two regimes:
//   1. "data" phases (Preamble, ReadPartBody) push bytes through the
//      StreamSearch, which streams non-needle bytes to onData and flips
//      `matched()` when the boundary needle has been observed in full.
//   2. "control" phases (AfterBoundary, ReadHeaders) consume bytes directly.
size_t MultipartBodyParser::consume(const char *buf, size_t len) {
	size_t i = 0;
	while (i < len && !_done && !_error) {
		if (_phase == Preamble || _phase == ReadPartBody) {
			size_t taken = _search.feed(buf + i, len - i);
			i += taken;
			if (_search.matched()) {
				if (_phase == ReadPartBody) closePartFile();
				_search.clearMatch();
				_phase = AfterBoundary;
				_afterTail.clear();
			} else {
				break;  // chunk exhausted without finding boundary
			}
			continue;
		}

		if (_phase == AfterBoundary) {
			while (i < len && _afterTail.size() < 2)
				_afterTail += buf[i++];
			if (_afterTail.size() < 2)
				break;
			if (_afterTail == "--") {
				_phase = Epilogue;
				_done = true;
				return i;
			}
			if (_afterTail != "\r\n") {
				_error = true;
				return i;
			}
			_headerLine.clear();
			_phase = ReadHeaders;
			continue;
		}

		if (_phase == ReadHeaders) {
			while (i < len) {
				char c = buf[i++];
				_headerLine += c;
				const size_t hl = _headerLine.size();
				if (hl >= 2 && _headerLine[hl - 2] == '\r' && _headerLine[hl - 1] == '\n') {
					if (hl == 2) {
						_headerLine.clear();
						openPartFile();
						_phase = ReadPartBody;
						break;
					}
					parseHeaderLine(_headerLine);
					_headerLine.clear();
				}
			}
			continue;
		}
	}
	return i;
}

void MultipartBodyParser::onData(const char *data, size_t len) {
	if (_phase == Preamble) return;  // discard preamble bytes
	if (_phase != ReadPartBody || !_partOpen) return;
	map<int, BodyFile>::iterator it = _files.find(_fileIndex);
	if (it != _files.end())
		it->second.write(data, len);
}

void MultipartBodyParser::openPartFile() {
	const string path = "/tmp/.servio_" + to_string(getmstime()) + "_" + to_string(_fileIndex) + "_upload.io";
	FILE *file = fopen(path.c_str(), "w+");
	if (!file) {
		_error = true;
		return;
	}
	_files[_fileIndex].adoptFile(file, path);
	_partOpen = true;
}

void MultipartBodyParser::closePartFile() {
	if (!_partOpen) return;
	map<int, BodyFile>::iterator it = _files.find(_fileIndex);
	if (it != _files.end() && it->second.file())
		fflush(it->second.file());
	_partOpen = false;
	_fileIndex++;
}

void MultipartBodyParser::parseHeaderLine(const string &line) {
	// `line` always ends with CRLF when we get here.
	if (line.size() < 2) return;
	const size_t end = line.size() - 2;
	const size_t colon = line.find(':');
	if (colon == string::npos || colon >= end) return;
	string key = line.substr(0, colon);
	string value = line.substr(colon + 1, end - colon - 1);
	trim(key);
	trim(value);
	_files[_fileIndex].addHeader(key, value);
}
