#ifndef SERVIO_RESPONSE_SENDER_HPP
#define SERVIO_RESPONSE_SENDER_HPP

#include "utility/socket.hpp"

class Response;

// Strategy interface for delivering a response body.
//
// `Response` keeps the protocol-level state (status, headers, keep-alive)
// and dispatches the body phase to a sender chosen at setup time. Each
// concrete sender owns the small per-strategy state machine (chunk size,
// range slice, CGI fd, etc.) and is a friend of `Response` so it can read
// the request-side context it needs.
//
// The two-method contract intentionally mirrors `BodyParser`:
//
//   - `prepareHeaders` runs once before the status line goes out. Use it
//     to compute Content-Length, Content-Range, Transfer-Encoding, ... It
//     returns false to defer sending (the CGI sender uses this while the
//     request body is still being received).
//   - `sendBody` is called once per writable poll; it must be safe to
//     call repeatedly and should set `RES_DONE` on the response when the
//     body has been fully delivered.
class ResponseSender {
   public:
	virtual ~ResponseSender();

	virtual bool prepareHeaders(Response &r) = 0;
	virtual void sendBody(Response &r, sockfd fd) = 0;
};

// In-memory or sendfile(2)-backed body of known length.
class LengthedSender : public ResponseSender {
   public:
	virtual bool prepareHeaders(Response &r);
	virtual void sendBody(Response &r, sockfd fd);
};

// HTTP/1.1 chunked transfer encoding wrapper around an iostream body.
class ChunkedSender : public ResponseSender {
   public:
	virtual bool prepareHeaders(Response &r);
	virtual void sendBody(Response &r, sockfd fd);
};

// Single-specifier `Range:` slice, served via sendfile(2) when possible.
class RangedSender : public ResponseSender {
   public:
	virtual bool prepareHeaders(Response &r);
	virtual void sendBody(Response &r, sockfd fd);
};

// CGI: parse the child's headers off `_cgiFd` once it's been written to,
// then stream the rest of its stdout to the socket.
class CGISender : public ResponseSender {
   public:
	virtual bool prepareHeaders(Response &r);
	virtual void sendBody(Response &r, sockfd fd);
};

// Multipart upload landing: rename tmp files to `upload_store`, then emit
// a small HTML summary as the response body.
class UploadSender : public ResponseSender {
   public:
	virtual bool prepareHeaders(Response &r);
	virtual void sendBody(Response &r, sockfd fd);
};

#endif
