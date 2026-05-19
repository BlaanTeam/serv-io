#ifndef __HELPERS_H__
#define __HELPERS_H__

#include <deque>
#include <functional>
#include <sstream>
#include <string>

using namespace std;

#include "helpers.tpp"

pair<bool, string> normpath(const string &path, const char sep = '/');
bool               iequalString(const string &s1, const string &s2);

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