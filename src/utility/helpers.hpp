#ifndef SERVIO_HELPERS_HPP
#define SERVIO_HELPERS_HPP

#include <deque>
#include <functional>
#include <sstream>
#include <string>

#include "result.hpp"

using namespace std;

#include "helpers.tpp"

// Normalize a path: collapses "." segments, applies "..", rejects paths that
// would escape the root. Returns None on invalid input, Some(normalized) on
// success. Trailing slash semantics are preserved.
servio::Option<string> normpath(const string &path, const char sep = '/');

bool iequalString(const string &s1, const string &s2);

void ltrim(string &value, const string &sep = " ");
void rtrim(string &value, const string &sep = " ");
void trim(string &value, const string &sep = " ");

class StringICaseCompare : binary_function<string, string, bool> {
	class CharICaseCompare;

   public:
	bool operator()(const std::string &s1, const std::string &s2) const;
};

string joinPath(const string &parentDir, const string &childDir);

size_t getFileSize(iostream *stream);

#endif