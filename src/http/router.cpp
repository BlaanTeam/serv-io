#include "router.hpp"

#include <sys/stat.h>

using namespace std;
#include <unistd.h>

#include "utility/helpers.hpp"

Handler::~Handler() {}

namespace {

// ----------------------------------------------------------------- gates --

// Reject malformed requests outright, and honor a virtual-server-level
// `return` redirect before any Location matching even runs.
class EarlyGate : public Handler {
   public:
	virtual Outcome handle(RoutingContext &ctx) {
		if (!ctx.req.valid()) {
			ctx.res.setupErrorResponse(ctx.req.statusCode(), ctx.virtualServer);
			return Handled;
		}
		if (ctx.virtualServer->isRedirectable()) {
			ctx.res.setupRedirectResponse(ctx.virtualServer->redirect(), ctx.virtualServer);
			return Handled;
		}
		return Pass;
	}
};

// Resolve the request path to a Location and run per-location gates:
//   - missing location           -> 404
//   - body exceeds client_max_*  -> 413
//   - location-level `return`    -> redirect
//   - method not in allow-list   -> 405
// On success, leaves `ctx.location` populated for downstream handlers.
class LocationGate : public Handler {
   public:
	virtual Outcome handle(RoutingContext &ctx) {
		Location *loc = ctx.virtualServer->match(ctx.req.path());
		if (!loc) {
			ctx.res.setupErrorResponse(NOT_FOUND, ctx.virtualServer);
			return Handled;
		}
		if (ctx.req.isTooLarge(loc->directives()["client_max_body_size"].value)) {
			ctx.res.setupErrorResponse(REQUEST_ENTITY_TOO_LARGE, ctx.virtualServer);
			return Handled;
		}
		if (loc->isRedirectable()) {
			ctx.res.setupRedirectResponse(loc->redirect(), loc);
			return Handled;
		}
		if (!loc->isAllowedMethod(ctx.req.method())) {
			ctx.res.setupErrorResponse(METHOD_NOT_ALLOWED, loc);
			return Handled;
		}
		ctx.location = loc;
		return Pass;
	}
};

// -------------------------------------------------------- dispatchers ----

// Fork + exec the configured CGI binary when the location is a CGI mount
// and the request path actually points at a script there.
class CGIDispatch : public Handler {
   public:
	virtual Outcome handle(RoutingContext &ctx) {
		Location *loc = ctx.location;
		const size_t locLen  = loc->location().length();
		const size_t pathLen = ctx.req.path().length();
		if (!loc->isCGI() || pathLen <= locLen)
			return Pass;

		servio::Result<CGI, std::string> r = CGI::create(loc, &ctx.req, &ctx.res);
		if (r.isErr())
			return Pass;

		CGI cgi = r.unwrap();
		*ctx.cgiPid = cgi.spawn(ctx.cgiFds, ctx.req.fileno());
		ctx.res.setupCGIResponse(ctx.cgiFds[0], &ctx.req);
		return Handled;
	}
};

// Multipart upload sink: the body parser has already streamed each part to
// a tmp file; UploadSender's `setupUploadBody` renames them under
// `upload_store`.
class UploadDispatch : public Handler {
   public:
	virtual Outcome handle(RoutingContext &ctx) {
		Location *loc = ctx.location;
		if (!loc->isUpload() || ctx.req.method() != POST)
			return Pass;
		ctx.res.setupUploadResponse(loc, &ctx.req);
		return Handled;
	}
};

// Terminal handler: serve a regular file (with sendfile), a directory
// listing, or an index page — whichever matches the request path and the
// location's autoindex/index settings. Always returns `Handled`.
class StaticServe : public Handler {
   public:
	virtual Outcome handle(RoutingContext &ctx) {
		Location   *loc  = ctx.location;
		std::string path = ctx.req.path();

		ctx.res.extractRange(ctx.req);

		struct stat fileStat;
		bzero(&fileStat, sizeof fileStat);
		if (!loc->found(path, fileStat)) {
			ctx.res.setupErrorResponse(NOT_FOUND, loc);
			return Handled;
		}

		if (S_ISDIR(fileStat.st_mode)) {
			const size_t pathLen = ctx.req.path().length();
			if (pathLen > 1 && ctx.req.path()[pathLen - 1] != '/') {
				Redirect redir(MOVED_PERMANENTLY, joinPath(ctx.req.path(), "/"), true);
				ctx.res.setupRedirectResponse(&redir, loc);
				return Handled;
			}
			const std::string indexPath = joinPath(path, loc->index());
			if (!access(indexPath.c_str(), F_OK | R_OK) && ctx.res.setupNormalResponse(indexPath))
				return Handled;
			if (loc->isAutoIndexable()) {
				ctx.res.setupDirectoryListing(path, ctx.req.path());
				return Handled;
			}
			ctx.res.setupErrorResponse(FORBIDDEN, loc);
			return Handled;
		}

		if (!access(path.c_str(), F_OK | R_OK) && ctx.res.setupNormalResponse(path))
			return Handled;

		ctx.res.setupErrorResponse(FORBIDDEN, loc);
		return Handled;
	}
};

}  // namespace

// ----------------------------------------------------------------- chain --

DefaultRouter::DefaultRouter() {
	_chain.push_back(new EarlyGate());
	_chain.push_back(new LocationGate());
	_chain.push_back(new CGIDispatch());
	_chain.push_back(new UploadDispatch());
	_chain.push_back(new StaticServe());
}

DefaultRouter::~DefaultRouter() {
	for (size_t i = 0; i < _chain.size(); ++i)
		delete _chain[i];
}

void DefaultRouter::route(RoutingContext &ctx) {
	for (size_t i = 0; i < _chain.size(); ++i) {
		if (_chain[i]->handle(ctx) == Handler::Handled)
			return;
	}
}
