#ifndef __RESPONSE_H__
#define __RESPONSE_H__

#define RES_INIT (1 << 0)
#define RES_HEADER (1 << 1)
#define RES_BODY (1 << 2)
#define RES_DONE (1 << 3)

#define LENGTHED_RES (1 << 0)
#define CHUNKED_RES (1 << 1)
#define RANGED_RES (1 << 2)
#define CGI_RES (1 << 3)
#define UPLOAD_RES (1 << 4)

#define INIT_LENGTH (1 << 0)
#define ONGOING_LENGTH (1 << 1)
#define DONE_LENGTH (1 << 2)

#include <sys/types.h>

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "./sio_header.hpp"
#include "./sio_http_codes.hpp"
#include "./sio_http_range.hpp"
#include "./sio_mime_types.hpp"
#include "./sio_request.hpp"
#include "core/sio_ast.hpp"
#include "http/sio_http_range.hpp"
#include "utility/sio_helpers.hpp"
#include "utility/sio_socket.hpp"
#include "utility/sio_utils.hpp"

#define TIMEOUT 15000
#define CHUNK_SIZE 1024

using namespace std;

class Response {
	stringstream _ss;

	bool   _isCustomStatusCode;
	short  _statusCode;
	string _statusStringCode;
	short  _type;
	short  _state;
	int    _length;
	short  _lengthState;

	iostream *_stream;
	int       _fd;

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