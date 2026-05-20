#ifndef SERVIO_HEADER_HPP
#define SERVIO_HEADER_HPP

#include <map>
#include <set>
#include <string>
#include <iostream>

#include "utility/helpers.hpp"
#include "utility/result.hpp"

using namespace std;

class Header : public map<string, set<string>, StringICaseCompare> {
   public:
	bool   found(const string& key) const;

	// Backwards-compat lookup — returns the (trimmed) first value, or an
	// empty string when the header is missing. Prefer `find()` in new code.
	string get(const string& key);

	// Rust-style lookup. Returns `None` when the header is absent, otherwise
	// `Some(value)` (trimmed). (Named `tryGet` to avoid shadowing the
	// inherited `std::map::find`.)
	servio::Option<string> tryGet(const string& key) const;

	void add(const string& key, const string& value);

	void display() {
		iterator it = begin();

		while (it != end()) {
			for (set<string>::iterator _it = it->second.begin(); _it != it->second.end(); ++_it)
				cerr << it->first << "=" << *_it << endl;
			it++;
		}
	}
};

#endif