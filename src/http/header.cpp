#include "header.hpp"

bool Header::found(const string &key) const {
	return find(key) != end();
}

string Header::get(const string &key) {
	string                value;
	set<string>::iterator it = (*this)[key].begin();
	if (it != (*this)[key].end())
		value = *it;
	trim(value);
	return value;
}

servio::Option<string> Header::tryGet(const string &key) const {
	const_iterator it = find(key);
	if (it == end() || it->second.empty())
		return servio::None<string>();
	string value = *it->second.begin();
	trim(value);
	return servio::Some(value);
}

void Header::add(const string &key, const string &value) {
	(*this)[key].insert(value);
}