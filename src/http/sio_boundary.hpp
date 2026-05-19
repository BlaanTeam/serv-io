#ifndef __BOUNDARY_H__
#define __BOUNDARY_H__

#define MULTIPART_FORM_DATA_STRING "multipart/form-data"
#define BOUNDARY_STRING "boundary"

#include <string>

#include "../utility/sio_helpers.hpp"

using namespace std;

// Parses a Content-Type header value of the form
//   multipart/form-data; boundary=<value>
// and stores the boundary, leaving a flag that tells the body parser whether
// multipart parsing should be engaged.
class Boundary {
	bool   _valid;
	string _value;

   public:
	Boundary();
	Boundary(const string &headerValue);

	bool          valid() const;
	const string &value() const;
};

#endif
