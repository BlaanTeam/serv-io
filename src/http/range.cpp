#include "range.hpp"

#include <cctype>

size_t RangeSpecifier::getContentLength(iostream *stream) {
	return getContentLength(getFileSize(stream));
}

size_t RangeSpecifier::getContentLength(size_t fileSize) {
	long long length = (long long)rangeEnd - (long long)rangeStart + 1;

	if (type == NOL)
		length = (long long)fileSize - (long long)rangeStart;
	else if (type & NOF)
		length = (long long)((rangeEnd < fileSize) ? rangeEnd : fileSize);
	return length < 0 ? 0 : (size_t)length;
}

void RangeSpecifier::setupSeek(iostream *stream) {
	if (type == NOF)
		stream->seekg(getFileSize(stream) - rangeEnd);
	else
		stream->seekg(rangeStart);
}

Range::Range() {}

const vector<RangeSpecifier> &Range::specifiers() const { return _specs; }
bool                          Range::empty()      const { return _specs.empty(); }

// Range: <unit>=<range-start>-
// Range: <unit>=-<suffix-length>
// Range: <unit>=<range-start>-<range-end>
// Range: <unit>=<range-start>-<range-end>, <unit>=<range-start>-<range-end>
servio::Result<Range, string> Range::parse(const string &headerValue) {
	typedef servio::Result<Range, string> R;

	string part(headerValue);
	trim(part);

	if (part.rfind("bytes=", 0) != 0)
		return R::err("unsupported range unit (only `bytes=` is accepted)");

	Range range;
	stringstream ss(part.substr(6));  // skip "bytes="
	while (getline(ss, part, ',')) {
		trim(part);
		servio::Result<RangeSpecifier, string> one = parseOne(part);
		if (one.isErr())
			return R::err(one.unwrapErr());
		range._specs.push_back(one.unwrap());
	}
	if (range._specs.empty())
		return R::err("range header has no specifiers");
	return R::ok(range);
}

static bool parseUnit(const string &value, size_t &out) {
	if (value.empty()) return false;
	for (size_t i = 0; i < value.size(); ++i)
		if (!isdigit((unsigned char)value[i]))
			return false;
	out = (size_t)stod(value);
	return true;
}

servio::Result<RangeSpecifier, string> Range::parseOne(const string &spec) {
	typedef servio::Result<RangeSpecifier, string> R;

	RangeSpecifier rs = {(size_t)-1, (size_t)-1, NON};

	if (spec.empty())
		return R::err("empty range specifier");

	if (spec[0] == '-') {
		if (!parseUnit(spec.substr(1), rs.rangeEnd))
			return R::err("malformed suffix range: " + spec);
		rs.type = NOF;
		return R::ok(rs);
	}

	const size_t dash = spec.find('-');
	if (dash == string::npos || !isdigit((unsigned char)spec[0]))
		return R::err("missing dash in range: " + spec);

	if (!parseUnit(spec.substr(0, dash), rs.rangeStart))
		return R::err("invalid range start in: " + spec);

	const string endTok = spec.substr(dash + 1);
	if (endTok.empty()) {
		rs.type = NOL;
		return R::ok(rs);
	}
	if (!parseUnit(endTok, rs.rangeEnd))
		return R::err("invalid range end in: " + spec);
	if (rs.rangeEnd < rs.rangeStart)
		return R::err("range end precedes start: " + spec);

	return R::ok(rs);
}
