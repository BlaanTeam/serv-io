#include "header.hpp"

using namespace std;

void Header::add(const string &key, const string &value) {
	_entries[key].insert(value);
}

void Header::erase(const string &key) {
	_entries.erase(key);
}

void Header::clear() {
	_entries.clear();
}

bool Header::found(const string &key) const {
	return _entries.find(key) != _entries.end();
}

servio::Option<string> Header::get(const string &key) const {
	const_iterator it = _entries.find(key);
	if (it == _entries.end() || it->second.empty())
		return servio::None<string>();
	string value = *it->second.begin();
	trim(value);
	return servio::Some(value);
}

void Header::setAll(const string &key, const ValueSet &values) {
	_entries[key] = values;
}

Header::iterator       Header::begin()       { return _entries.begin(); }
Header::iterator       Header::end()         { return _entries.end(); }
Header::const_iterator Header::begin() const { return _entries.begin(); }
Header::const_iterator Header::end()   const { return _entries.end(); }

bool Header::empty() const { return _entries.empty(); }

void Header::display() const {
	for (const_iterator it = _entries.begin(); it != _entries.end(); ++it)
		for (ValueSet::const_iterator v = it->second.begin(); v != it->second.end(); ++v)
			cerr << it->first << "=" << *v << endl;
}
