#include "helpers.hpp"

pair<bool, string> normpath(const string &path, const char sep) {
	deque<string> components;
	stringstream  ss(path);
	string        component;

	if (path[0] != sep)
		return make_pair(false, "");

	getline(ss, component, sep);  // skip empty component !!
	while (getline(ss, component, sep)) {
		if (component == ".")
			continue;
		else if (component == "..") {
			if (components.empty())
				return make_pair(false, "");
			components.pop_back();
		} else
			components.push_back(component);
	}
	string tmp = "/";
	for (size_t idx = 0; idx < components.size(); idx++) {
		if (idx != 0)
			tmp += "/";
		tmp += components[idx];
	}
	if (path.length() && path[path.length() - 1] == '/')
		tmp = joinPath(tmp, "/");
	return make_pair(true, tmp);
}

bool iequalString(const string &s1, const string &s2) {
	if (s1.length() != s2.length())
		return false;
	for (size_t idx = 0; idx < s1.length(); idx++)
		if (toupper(s1[idx]) != toupper(s2[idx]))
			return false;
	return true;
}

void ltrim(string &value, const string &sep) {
	string::iterator it = value.begin();
	while (it != value.end() && ::strchr(sep.c_str(), *it))
		it++;
	value.erase(value.begin(), it);
}

void rtrim(string &value, const string &sep) {
	string::reverse_iterator it = value.rbegin();
	while (it != value.rend() && ::strchr(sep.c_str(), *it))
		it++;
	value.erase(value.length() - (it - value.rbegin()));
}

void trim(string &value, const string &sep) {
	ltrim(value, sep);
	rtrim(value, sep);
}

class StringICaseCompare::CharICaseCompare : public binary_function<unsigned char, unsigned char, bool> {
   public:
	bool operator()(const unsigned char &c1, const unsigned char &c2) const {
		return ::tolower(c1) < ::tolower(c2);
	}
};

bool StringICaseCompare::operator()(const std::string &s1, const std::string &s2) const {
	return lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), CharICaseCompare());
}

string joinPath(const string &parentDir, const string &childDir) {
	if (parentDir.empty() || childDir.empty())
		return parentDir + childDir;
	string path(parentDir);
	if (parentDir.back() != '/' && childDir.front() != '/')
		path += "/";
	else if (parentDir.back() == '/' && childDir.front() == '/')
		return parentDir.substr(0, parentDir.length() - 1) + childDir;
	return path + childDir;
}

size_t getFileSize(iostream *stream) {
	size_t currSeek = stream->tellg();
	stream->seekg(0, ios::end);

	size_t count = stream->tellg();
	stream->seekg(currSeek);
	return count;
}