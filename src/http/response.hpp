#ifndef SERVIO_RESPONSE_HPP
#define SERVIO_RESPONSE_HPP

#include <sys/types.h>

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "./header.hpp"
#include "./mime_types.hpp"
#include "./range.hpp"
#include "./request.hpp"
#include "./response_sender.hpp"
#include "./status_codes.hpp"
#include "core/ast.hpp"
#include "utility/helpers.hpp"
#include "utility/socket.hpp"
#include "utility/utils.hpp"

using namespace std;

// Response writer state. Flags are bitwise-combined and tested with &.
enum ResponseState {
	RES_INIT   = 1 << 0,
	RES_HEADER = 1 << 1,
	RES_BODY   = 1 << 2,
	RES_DONE   = 1 << 3
};

// Strategy tag — `Response` picks one at setup time and dispatches send()
// accordingly. (Kept as bit flags for legacy mask-checking code.)
enum ResponseType {
	LENGTHED_RES = 1 << 0,
	CHUNKED_RES  = 1 << 1,
	RANGED_RES   = 1 << 2,
	CGI_RES      = 1 << 3,
	UPLOAD_RES   = 1 << 4
};

// Ranged-body sub-state — tracks where we are inside a single Range slice.
enum RangedLengthState {
	INIT_LENGTH    = 1 << 0,
	ONGOING_LENGTH = 1 << 1,
	DONE_LENGTH    = 1 << 2
};

#define TIMEOUT 15000
#define CHUNK_SIZE 1024

class ResponseSender;
class LengthedSender;
class ChunkedSender;
class RangedSender;
class CGISender;
class UploadSender;

class Response {
	friend class ResponseSender;
	friend class LengthedSender;
	friend class ChunkedSender;
	friend class RangedSender;
	friend class CGISender;
	friend class UploadSender;

	stringstream _headerBuffer;

	bool   _isCustomStatusCode;
	short  _statusCode;
	string _statusStringCode;
	short  _type;
	short  _state;
	int    _length;
	short  _rangePhase;

	// Strategy: body-delivery is delegated to a sender chosen at setup time.
	ResponseSender *_sender;

	iostream *_stream;
	int       _cgiFd;

	// sendfile() path: when serving a regular file we skip the iostream layer
	// and ask the kernel to copy file -> socket directly. `_fileFd` is -1 when
	// the response is in-memory (stringstream-backed) or fstream-backed.
	int       _fileFd;
	off_t     _filePos;
	off_t     _fileLen;

	Range _range;

	LocationContext<Type> *_location;
	Request               *_req;

	Header _headers;
	bool   _keepAlive;

   public:
	Response();
	Response(const short &statusCode, bool keepAlive = true);
	Response(const Response &copy);
	Response &operator=(const Response &rhs);
	~Response();

	void init();
	void prepare(void);

	// Setters

	void setStatusCode(const short &statusCode);
	void setState(const int &state);
	void setStream(iostream *stream);
	void addHeader(const string &name, const string &value);

	void extractRange(Request &req);
	void setConnectionStatus(bool keepAlive = true);

	bool keepAlive(void) const;

	void send(const sockfd &fd);

	void setupErrorResponse(const int &statusCode, MainContext<Type> *ctx, bool isBuiltIn = true);
	void setupRedirectResponse(Redirect *redir, MainContext<Type> *ctx);
	void setupDirectoryListing(const string &path, const string &title);
	// Opens `path` with open(2) and wires the response for sendfile(2) delivery.
	// Returns false if the file cannot be opened.
	bool setupNormalResponse(const string &path);
	void setupCGIResponse(const int &fd, Request *req);
	void setupUploadResponse(LocationContext<Type> *location, Request *req);

	bool match(const int &state) const;

	void reset(void);

	// ----------------------------------------------------------- Builder ---
	//
	// Fluent setup for Response. The plain `setStatusCode`/`addHeader`/...
	// methods still work, but the setup helpers below (setupErrorResponse,
	// setupRedirectResponse, ...) compose a `Builder` instead of mutating
	// state via half a dozen individual calls. Each builder method returns
	// *this so callers can chain, and `apply()` emits the project's standard
	// header bundle (Server/Date/Connection/Keep-Alive/Accept-Ranges).
	class Builder {
	   public:
		explicit Builder(Response &target);

		Builder &status(int code);
		Builder &keepAlive(bool keep = true);
		Builder &contentType(const string &value);
		Builder &header(const string &name, const string &value);
		Builder &body(iostream *stream);
		Builder &asLengthed();
		Builder &asChunked();
		Builder &asRanged();
		Builder &asCGI();
		Builder &asUpload();

		// Higher-level convenience for the two senders that need extra
		// request-side context (the CGI child fd, the upload's destination
		// location). They install the matching sender, attach the context,
		// and pick a sensible status.
		Builder &cgi(int childFd, Request *req);
		Builder &upload(LocationContext<Type> *location, Request *req);

		void apply();

	   private:
		Response &_r;
	};

	Builder build();

   private:
	void sendLengthedBody(const sockfd &fd);
	void setupLengthedBody(void);

	void sendChunkedBody(const sockfd &fd);
	void setupChunkedBody(void);

	void sendRangedBody(const sockfd &fd);
	void setupRangedBody(void);

	void parseHeaders(stringstream &ss);
	void changeState(const int &state);

	void sendCGIBody(const sockfd &fd);
	bool setupCGIBody();

	void sendKiloByte(const sockfd &fd);
	void sendLessThanKiloByte(const sockfd &fd);

	void sendUploadBody(const sockfd &fd);
	bool setupUploadBody();
};

#endif