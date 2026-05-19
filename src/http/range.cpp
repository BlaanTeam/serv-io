#include "range.hpp"

size_t RangeSpecifier::getContentLength(iostream *stream) {
	return getContentLength(getFileSize(stream));
}

size_t RangeSpecifier::getContentLength(size_t fileSize) {
	long long contentLength = rangeEnd - rangeStart + 1;

	if (type == NOL)
		contentLength = fileSize - rangeStart;
	else if (type & NOF)
		contentLength = min(fileSize, rangeEnd);
	return contentLength < 0 ? 0 : contentLength;
}

void RangeSpecifier::setupSeek(iostream *stream) {
	if (type == NOL)
		stream->seekg(rangeStart);
	else if (type == NOF)
		stream->seekg(getFileSize(stream) - rangeEnd);
	else
		stream->seekg(rangeStart);
}

Range::Range() {
	_valid = false;
}

// Range: <unit>=<range-start>-
// Range: <unit>=-<suffix-length>
// Range: <unit>=<range-start>-<range-end>
// Range: <unit>=<range-start>-<range-end>, <unit>=<range-start>-<range-end>

Range::Range(const string &value) {
	stringstream ss;
	string       part(value);

	trim(part);
	if (part.rfind("bytes=", 0) != 0)
		goto invalid;
	ss.str(part.substr(6));  // skip "bytes="
	_valid = true;
	while (getline(ss, part, ',')) {
		trim(part);
		rangeSpecifiers.push_back(parse(part));
		if (!valid())
			return;
	}
	return;
invalid:
	_valid = false;
}

static int parseUnit(const string &value, size_t &unit) {
	if (!every(value, ::isdigit))
		return -1;
	unit = value.length() ? stod(value) : -1;
	return value.length();
}

// Sorry smart hard coding !!

RangeSpecifier Range::parse(const string &value) {
	RangeSpecifier rs = {-1, -1, NON};
	string         tmp = value;

	_valid = true;

	if (value.rfind("-", 0) == 0) {
		if (parseUnit(tmp.substr(1), rs.rangeEnd) == -1)
			goto invalid;
		rs.type = NOF;
	} else if (isdigit(value[0])) {
		string token = tmp.substr(0, tmp.find("-"));
		int    n = parseUnit(token, rs.rangeStart);
		if (n == -1 || token.length() == tmp.length())  // in case -> bytes=100
			goto invalid;
		token = tmp.substr(token.length() + 1);
		n = parseUnit(token, rs.rangeEnd);
		rs.type = !n ? NOL : rs.type;
		if (n == -1 || (rs.rangeEnd < rs.rangeStart && rs.type & NON))
			goto invalid;
	} else
		goto invalid;
	return rs;
invalid:
	_valid = false;
	return RangeSpecifier();
}

bool Range::valid(void) const {
	return _valid;
}

vector<RangeSpecifier> Range::getRangeSpecifiers(void) const {
	return rangeSpecifiers;
}