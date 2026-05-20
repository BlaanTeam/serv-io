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

class StringICaseCompare : std::binary_function<std::string, std::string, bool> {
	class CharICaseCompare;

   public:
	bool operator()(const std::string &s1, const std::string &s2) const;
};

std::string joinPath(const std::string &parentDir, const std::string &childDir);

std::size_t getFileSize(std::iostream *stream);

#endif
