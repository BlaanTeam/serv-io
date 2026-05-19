#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <map>
#include <string>

#include "./sio_header.hpp"
#include "./sio_http_codes.hpp"
#include "./sio_request_body.hpp"
#include "http/sio_http_range.hpp"
#include "utility/sio_helpers.hpp"
#include "utility/sio_utils.hpp"

using namespace std;

#define REQ_INIT (1 << 0)
#define REQ_LINE (1 << 1)
#define REQ_HEADER (1 << 2)
#define REQ_BODY (1 << 3)
#define REQ_DONE (1 << 4)
#define REQ_INVALID (1 << 5)

#define REQ_MAX_URI 8192
#define REQ_MAX_HEADER_LINE 8192

class Body;

class Request {
	short _state;
	short _statusCode;

	HttpMethod _method;
	string     _path;
	string     _query;
	string     _line;  // accumulating request line / current header line

	Header _headers;
	Body   _body;

   public:
	typedef Header::iterator headerIter;

	Request();
	Request(const Request &copy);
	Request &operator=(const Request &rhs);
	~Request();

	// Feed bytes received from the client socket. Returns the number of bytes
	// consumed (may be < len if parsing is complete or invalid).
	size_t consume(const char *buf, size_t len);

	// Getters
	string             getPath(void) const;
	string             getQuery(void) const;
	short              getState(void) const;
	int                getStatusCode() const;
	int                getFileno() const;
	HttpMethod         getMethod(void) const;
	bool               match(const int &state) const;
	Header            &getHeaders(void);
	map<int, BodyFile> &getBodyFiles();
	Range              getRange();

	void reset(void);
	void closeBodyFile(void);

	bool valid() const;
	bool isTooLarge(const int &clientMaxSize);

   private:
	// Consume one byte while in REQ_INIT (skip leading CRLF/whitespace).
	// Returns true to continue the outer loop, false to advance to REQ_LINE.
	bool skipLeading(char c);

	void parseRequestLine();
	void parseHeaderLine();
	void onHeadersComplete();

	void changeState(short state);
	void fail(short statusCode);
};

#endif
