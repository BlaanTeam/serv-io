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

void Header::add(const string &key, const string &value) {
	(*this)[key].insert(value);
}