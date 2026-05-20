#ifndef SERVIO_MIME_TYPES_HPP
#define SERVIO_MIME_TYPES_HPP

#include <map>
#include <string>

#include "utility/helpers.hpp"

#define DEFAULT_MIME_TYPE "text/plain"

using namespace std;

class MimeType : public map<string, const char *, StringICaseCompare> {
	typedef map<string, const char *, StringICaseCompare> Base;

   public:
	MimeType();

	mapped_type &choiceMimeType(const string &path);

	mapped_type &operator[](const key_type &key);
};

extern MimeType mimeTypes;

#endif