#ifndef __HEADER_H__
#define __HEADER_H__

#include <map>
#include <set>
#include <string>
#include <iostream>

#include "utility/helpers.hpp"

using namespace std;

class Header : public map<string, set<string>, StringICaseCompare> {
   public:
	bool   found(const string& key) const;
	string get(const string& key);

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