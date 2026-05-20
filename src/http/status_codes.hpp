#ifndef SERVIO_STATUS_CODES_HPP
#define SERVIO_STATUS_CODES_HPP

#include <dirent.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <iomanip>
#include <map>
#include <sstream>
#include <string>

#include "utility/helpers.hpp"

#define CONTINUE 100
#define SWITCHING_PROTOCOLS 101

#define OK 200
#define CREATED 201
#define ACCEPTED 202
#define NON_AUTHORITATIVE_INFORMATION 203
#define NO_CONTENT 204
#define RESET_CONTENT 205
#define PARTIAL_CONTENT 206

#define MULTIPLE_CHOICES 300
#define MOVED_PERMANENTLY 301
#define FOUND 302
#define SEE_OTHER 303
#define NOT_MODIFIED 304
#define USE_PROXY 305
#define TEMPORARY_REDIRECT 307

#define BAD_REQUEST 400
#define UNAUTHORIZED 401
#define PAYMENT_REQUIRED 402
#define FORBIDDEN 403
#define NOT_FOUND 404
#define METHOD_NOT_ALLOWED 405
#define NOT_ACCEPTABLE 406
#define PROXY_AUTHENTICATION_REQUIRED 407
#define REQUEST_TIMEOUT 408
#define CONFLICT 409
#define GONE 410
#define LENGTH_REQUIRED 411
#define PRECONDITION_FAILED 412
#define REQUEST_ENTITY_TOO_LARGE 413
#define REQUEST_URI_TOO_LONG 414
#define UNSUPPORTED_MEDIA_TYPE 415
#define REQUESTED_RANGE_NOT_SATISFIABLE 416
#define EXPECTATION_FAILED 417

#define INTERNAL_SERVER_ERROR 500
#define NOT_IMPLEMENTED 501
#define BAD_GATEWAY 502
#define SERVICE_UNAVAILABLE 503
#define GATEWAY_TIMEOUT 504
#define HTTP_VERSION_NOT_SUPPORTED 505

class HttpStatusCode : public std::map<short, std::string> {
   public:
	HttpStatusCode();
};

extern HttpStatusCode httpStatusCodes;


extern int httpMethodCount;

enum HttpMethod {
	GET = (1 << 0),
	HEAD = (1 << 1),
	POST = (1 << 2),
	PUT = (1 << 3),
	DELETE = (1 << 4),
	CONNECT = (1 << 5),
	OPTIONS = (1 << 6),
	TRACE = (1 << 7),
	UNKNOWN = (1 << 8)
};

extern std::string httpMethods[8];

#define CR "\r"
#define LF "\n"
#define CRLF "\r\n"

#define HTTP_VERSION "HTTP/1.1"

#define NAME "ServIo"
#define VERSION "1.0.0"

std::iostream *buildResponseBody(const short &statusCode);

std::iostream *buildDirectoryListing(const std::string &path, const std::string &title);

#endif