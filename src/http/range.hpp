#ifndef SERVIO_RANGE_HPP
#define SERVIO_RANGE_HPP

#include <sstream>
#include <string>
#include <vector>

#include "utility/helpers.hpp"
#include "utility/result.hpp"

using namespace std;

enum UnitType {
	NON = (1 << 0),
	NOF = (1 << 1),
	NOL = (1 << 2)
};

struct RangeSpecifier {
	size_t   rangeStart;
	size_t   rangeEnd;
	UnitType type;

	size_t getContentLength(iostream *stream);
	size_t getContentLength(size_t fileSize);
	void   setupSeek(iostream *stream);
};

class Range {
   public:
	// Parses an HTTP `Range:` header value (without the field name). Returns
	// `Ok(range)` on success, `Err(reason)` on protocol mismatch or bad
	// syntax. A request without a `Range:` header should never reach this
	// function; callers must check the header is present first.
	static servio::Result<Range, string> parse(const string &headerValue);

	Range();                                       // empty range (no specifiers)

	const vector<RangeSpecifier> &specifiers() const;
	bool                          empty() const;

   private:
	static servio::Result<RangeSpecifier, string> parseOne(const string &spec);

	vector<RangeSpecifier> _specs;
};

#endif
