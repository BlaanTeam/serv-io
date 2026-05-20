#ifndef SERVIO_BOUNDARY_HPP
#define SERVIO_BOUNDARY_HPP

#define MULTIPART_FORM_DATA_STRING "multipart/form-data"
#define BOUNDARY_STRING "boundary"

#include <string>

#include "../utility/helpers.hpp"
#include "../utility/result.hpp"

// Parses a Content-Type header of the form
//     multipart/form-data; boundary=<value>
// Exposed as a Rust-style `Result` so callers must handle the parse error
// explicitly instead of consulting a `.valid()` flag.
class Boundary {
   public:
	static servio::Result<Boundary, std::string> parse(const std::string &headerValue);

	Boundary();                                   // empty boundary, useful as a sentinel
	explicit Boundary(const std::string &value);  // build from an already-extracted value

	const std::string &value() const;
	bool               empty() const;

   private:
	std::string _value;
};

#endif
