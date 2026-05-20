#ifndef SERVIO_HELPERS_HPP
#define SERVIO_HELPERS_HPP

#include <deque>
#include <functional>
#include <sstream>
#include <string>

#include "result.hpp"

#include "helpers.tpp"

// Normalize a path: collapses "." segments, applies "..", rejects paths that
// would escape the root. Returns None on invalid input, Some(normalized) on
// success. Trailing slash semantics are preserved.
servio::Option<std::string> normpath(const std::string &path, const char sep = '/');

bool iequalString(const std::string &s1, const std::string &s2);

void ltrim(std::string &value, const std::string &sep = " ");
void rtrim(std::string &value, const std::string &sep = " ");
void trim(std::string &value, const std::string &sep = " ");

// Case-insensitive `std::less`-equivalent suitable for use as a
// std::map / std::set comparator. `binary_function` was the C++98
// hook for this; deprecated in C++11, gone in C++17 — typed members
// do the same job.
class StringICaseCompare {
   public:
	using is_transparent = void;   // allow heterogeneous lookups in C++14
	using first_argument_type  = std::string;
	using second_argument_type = std::string;
	using result_type          = bool;

	bool operator()(const std::string &s1, const std::string &s2) const;

   private:
	class CharICaseCompare;
};

std::string joinPath(const std::string &parentDir, const std::string &childDir);

std::size_t getFileSize(std::iostream *stream);

#endif
