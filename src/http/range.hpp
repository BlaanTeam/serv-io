#ifndef SERVIO_RANGE_HPP
#define SERVIO_RANGE_HPP

#include <sstream>
#include <string>
#include <vector>

#include "utility/helpers.hpp"
#include "utility/result.hpp"

enum UnitType {
	NON = (1 << 0),
	NOF = (1 << 1),
	NOL = (1 << 2)
};

struct RangeSpecifier {
	std::size_t rangeStart;
	std::size_t rangeEnd;
	UnitType    type;

	std::size_t contentLength(std::iostream *stream);
	std::size_t contentLength(std::size_t fileSize);
	void        setupSeek(std::iostream *stream);
};

class Range {
   public:
	// Parses an HTTP `Range:` header value (without the field name). Returns
	// `Ok(range)` on success, `Err(reason)` on protocol mismatch or bad
	// syntax. A request without a `Range:` header should never reach this
	// function; callers must check the header is present first.
	static servio::Result<Range, std::string> parse(const std::string &headerValue);

	Range();   // empty range (no specifiers)

	const std::vector<RangeSpecifier> &specifiers() const;
	bool                               empty() const;

   private:
	static servio::Result<RangeSpecifier, std::string> parseOne(const std::string &spec);

	std::vector<RangeSpecifier> _specs;
};

#endif
