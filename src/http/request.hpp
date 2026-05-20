#ifndef SERVIO_REQUEST_HPP
#define SERVIO_REQUEST_HPP

#include <map>
#include <string>

#include "./body.hpp"
#include "./header.hpp"
#include "./range.hpp"
#include "./status_codes.hpp"
#include "utility/helpers.hpp"
#include "utility/utils.hpp"

using namespace std;

// Request parser state. Flags are bitwise-combined and tested with &.
enum RequestState {
	REQ_INIT    = 1 << 0,   // before the first non-whitespace byte
	REQ_LINE    = 1 << 1,   // accumulating the start-line
	REQ_HEADER  = 1 << 2,   // accumulating header lines
	REQ_BODY    = 1 << 3,   // body parser is consuming bytes
	REQ_DONE    = 1 << 4,   // request fully parsed
	REQ_INVALID = 1 << 5    // protocol error; _statusCode holds the right 4xx
};

enum RequestLimits {
	REQ_MAX_URI         = 8192,
	REQ_MAX_HEADER_LINE = 8192
};

class Request {
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
	string              getPath(void) const;
	string              getQuery(void) const;
	short               getState(void) const;
	int                 getStatusCode() const;
	int                 getFileno() const;
	HttpMethod          getMethod(void) const;
	bool                match(const int &state) const;
	Header             &getHeaders(void);
	map<int, BodyFile> &getBodyFiles();
	Range               getRange();

	void reset(void);
	void closeBodyFile(void);

	bool valid() const;
	bool isTooLarge(const int &clientMaxSize);

   private:
	void parseRequestLine();
	void parseHeaderLine();
	void onHeadersComplete();

	void changeState(short state);
	void fail(short statusCode);

	short      _state;
	short      _statusCode;
	HttpMethod _method;
	string     _path;
	string     _query;
	string     _line;     // current request line / current header line being assembled
	Header     _headers;
	Body       _body;
};

#endif
