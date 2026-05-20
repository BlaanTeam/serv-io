#ifndef SERVIO_HEADER_HPP
#define SERVIO_HEADER_HPP

#include <iostream>
#include <map>
#include <set>
#include <string>

#include "utility/helpers.hpp"
#include "utility/result.hpp"

using namespace std;

// HTTP header bag with case-insensitive key lookup. Multi-value headers
// (e.g. duplicate `Set-Cookie`) collapse into a `set<string>` per key.
//
// Designed as composition-over-inheritance — earlier versions inherited
// from `std::map<...>` publicly, which leaked the entire container API
// (operator[], insert(value_type), upper_bound, ...) and made the
// surface area accidentally enormous. The current shape exposes only the
// methods this project actually uses.
class Header {
   public:
	typedef set<string>                                       ValueSet;
	typedef map<string, ValueSet, StringICaseCompare>         Entries;
	typedef Entries::iterator                                 iterator;
	typedef Entries::const_iterator                           const_iterator;

	void add(const string &key, const string &value);
	void erase(const string &key);
	void clear();

	bool found(const string &key) const;

	// Single-value lookup. Returns the trimmed first value, or `None` when
	// the header is absent (or its value set is empty). Replaces the old
	// `""`-sentinel returning `get()`.
	servio::Option<string> get(const string &key) const;

	// Replace (or create) the multi-value set associated with `key`.
	void setAll(const string &key, const ValueSet &values);

	// Iteration — kept so callers can walk every header (CGI env, prepare()).
	iterator       begin();
	iterator       end();
	const_iterator begin() const;
	const_iterator end()   const;
	bool           empty() const;

	void display() const;

   private:
	Entries _entries;
};

#endif
