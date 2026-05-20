#include "boundary.hpp"

#include <sstream>

Boundary::Boundary() {}
Boundary::Boundary(const string &value) : _value(value) {}

const string &Boundary::value() const { return _value; }
bool          Boundary::empty() const { return _value.empty(); }

servio::Result<Boundary, string> Boundary::parse(const string &headerValue) {
	typedef servio::Result<Boundary, string> R;

	stringstream ss(headerValue);
	string       part;

	if (!getline(ss, part, ';'))
		return R::err("Content-Type is empty");
	trim(part);
	if (!iequalString(part, MULTIPART_FORM_DATA_STRING))
		return R::err("media type is not " MULTIPART_FORM_DATA_STRING);

	if (!getline(ss, part, '\0'))
		return R::err("missing boundary parameter");
	trim(part);

	stringstream kv(part);
	string       key;
	if (!getline(kv, key, '='))
		return R::err("malformed boundary parameter");
	trim(key);
	if (!iequalString(key, BOUNDARY_STRING))
		return R::err("expected `boundary=` parameter");

	string value;
	if (!getline(kv, value, '\0'))
		return R::err("missing boundary value");
	trim(value);
	trim(value, "\"");
	if (value.empty())
		return R::err("boundary value is empty");

	return R::ok(Boundary(value));
}
