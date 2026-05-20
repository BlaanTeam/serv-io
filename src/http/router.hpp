#ifndef SERVIO_ROUTER_HPP
#define SERVIO_ROUTER_HPP

#include <sys/types.h>

#include <memory>
#include <vector>

#include "./cgi.hpp"
#include "./request.hpp"
#include "./response.hpp"
#include "core/config.hpp"

// Pingora-flavored request routing.
//
// `resolveResponse` used to be a single-function decision tree:
//     if invalid -> error;
//     if vs.redirect -> redirect;
//     if !location -> 404;
//     ... etc.
//
// That works, but it's not extensible — adding a new request handler
// (rate-limit guard, auth middleware, hot-path logger) means surgery on a
// fixed sequence of `if` branches. The Handler interface lets the
// resolution chain be assembled from focused, independently-testable
// units; the Router walks them in order and stops at the first one that
// installs a response.

// Context shared by every handler. Holds the request, the response slot,
// the matched virtual server, and the cubbyholes the CGI handler uses to
// hand the child-fd back to the Client.
struct RoutingContext {
	Request       &req;
	Response      &res;
	VirtualServer *virtualServer;
	Location      *location;     // populated by LocationGate; null beforehand
	int           *cgiFds;       // optional sink for CGI fork pipe
	pid_t         *cgiPid;       // optional sink for CGI child pid

	RoutingContext(Request &r, Response &re, VirtualServer *vs, int *fds, pid_t *pid)
	    : req(r), res(re), virtualServer(vs), location(NULL), cgiFds(fds), cgiPid(pid) {}
};

class Handler {
   public:
	enum Outcome {
		Handled,    // installed a response; routing stops
		Pass        // not this handler's job; try the next one
	};

	virtual ~Handler();
	virtual Outcome handle(RoutingContext &ctx) = 0;
};

// Built-in chain that reproduces ServIO's default routing behavior:
//
//   EarlyGate       (invalid request, virtual-server-level redirect)
//   LocationGate    (locate the matching Location, then redirect / 413 / 405)
//   CGIDispatch     (run as CGI when the location is configured for it)
//   UploadDispatch  (multipart upload to configured upload_store)
//   StaticServe     (regular file, directory listing, or 404/403)
//
// Each handler does exactly one thing; the router stops at the first
// `Handled`.
class DefaultRouter {
   public:
	DefaultRouter();
	~DefaultRouter() = default;

	void route(RoutingContext &ctx);

	DefaultRouter(const DefaultRouter &)            = delete;
	DefaultRouter &operator=(const DefaultRouter &) = delete;

   private:
	std::vector<std::unique_ptr<Handler> > _chain;
};

#endif
