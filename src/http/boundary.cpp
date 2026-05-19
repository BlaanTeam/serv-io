#include "boundary.hpp"

#include <sstream>

Boundary::Boundary() : _valid(false) {}

Boundary::Boundary(const string &headerValue) : _valid(false) {
	stringstream ss(headerValue);
	string       part;

	if (!getline(ss, part, ';'))
		return;
	trim(part);
	if (!iequalString(part, MULTIPART_FORM_DATA_STRING))
		return;

	if (!getline(ss, part, '\0'))
		return;
	trim(part);

	stringstream kv(part);
	string       key;
	if (!getline(kv, key, '='))
		return;
	trim(key);
	if (!iequalString(key, BOUNDARY_STRING))
		return;

	if (!getline(kv, _value, '\0'))
		return;
	trim(_value);
	trim(_value, "\"");
	if (_value.empty())
		return;
	_valid = true;
}

bool Boundary::valid() const {
	return _valid;
}

const string &Boundary::value() const {
	return _value;
}
