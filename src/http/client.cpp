#include "client.hpp"

Client::Client() {
	_fds[0] = -1;
	_fds[1] = -1;
	_time = getmstime();
}

Client::Client(const pair<sockfd, Address> &connection) {
	_fds[0] = -1;
	_fds[1] = -1;
	_time = getmstime();
	_connection = connection;
}

Client::~Client() {
	close(_fds[0]);
}

// Setters

void Client::setTime(const long long &time) {
	_time = time;
}

void Client::setPollFd(const sockfd &fd, PollFd &pfd) {
	PollFd::iterator it = pfd.get(fd);
	if (it != pfd.end())
		_pfd = &(*it);
}

// Getters

bool Client::timedOut(void) const {
	return getmstime() - _time > TIMEOUT;
}

// Top-level connection handler. Drains the recv buffer into the request
// parser, decides on a Response, then sends what it can.
bool Client::handleRequest(const char *buf, size_t len) {
	reset();
	_req.consume(buf, len);

	VirtualServer *virtualServer = config.match(Address(_connection.first),
	                                            _req.headers().get("Host").unwrapOr(""));
	_ctx = virtualServer;

	const bool invalid    = !_req.valid();
	const bool readyToAct = invalid || _req.match(REQ_BODY | REQ_DONE);
	if (!readyToAct) {
		togglePollOut();
		return isPurgeable();
	}

	if (_res.match(RES_INIT))
		resolveResponse(virtualServer);

	_req.closeBodyFile();
	_res.send(_connection.first);

	if (waitForCgi()) return false;
	if (!_res.match(RES_DONE)) setTime(getmstime());

	togglePollOut();
	return isPurgeable();
}

// Decides which Response to install. Every branch installs exactly one
// response and then returns — no goto, no fallthrough.
void Client::resolveResponse(VirtualServer *virtualServer) {
	if (!_req.valid())
		return _res.setupErrorResponse(_req.statusCode(), virtualServer);

	Location *location = virtualServer->match(_req.path());
	_ctx = location;

	if (virtualServer->isRedirectable())
		return _res.setupRedirectResponse(virtualServer->redirect(), virtualServer);
	if (!location)
		return _res.setupErrorResponse(NOT_FOUND, virtualServer);
	if (_req.isTooLarge(location->directives()["client_max_body_size"].value))
		return _res.setupErrorResponse(REQUEST_ENTITY_TOO_LARGE, virtualServer);
	if (location->isRedirectable())
		return _res.setupRedirectResponse(location->redirect(), location);
	if (!location->isAllowedMethod(_req.method()))
		return _res.setupErrorResponse(METHOD_NOT_ALLOWED, location);

	const size_t locationLength = location->location().length();
	const size_t pathLength     = _req.path().length();

	if (location->isCGI() && pathLength > locationLength && tryCGI(location))
		return;
	if (location->isUpload() && _req.method() == POST)
		return _res.setupUploadResponse(location, &_req);

	resolveStaticFile(location, _req.path());
}

// Spawn the CGI child and install a CGISender. Returns false if the
// request doesn't match this location's CGI configuration.
bool Client::tryCGI(Location *location) {
	servio::Result<CGI, string> r = CGI::create(location, &_req, &_res);
	if (r.isErr())
		return false;
	CGI cgi = r.unwrap();
	_pid = cgi.spawn(_fds, _req.fileno());
	_res.setupCGIResponse(_fds[0], &_req);
	return true;
}

// Serve a regular file, a directory listing, or an index page — whichever
// matches the request path and the location's autoindex/index settings.
void Client::resolveStaticFile(Location *location, string path) {
	_res.extractRange(_req);

	struct stat fileStat;
	bzero(&fileStat, sizeof fileStat);
	if (!location->found(path, fileStat))
		return _res.setupErrorResponse(NOT_FOUND, location);

	if (S_ISDIR(fileStat.st_mode)) {
		// Redirect directory requests that lack a trailing slash.
		const size_t pathLength = _req.path().length();
		if (pathLength > 1 && _req.path()[pathLength - 1] != '/') {
			Redirect redir(MOVED_PERMANENTLY, joinPath(_req.path(), "/"), true);
			return _res.setupRedirectResponse(&redir, location);
		}

		const string indexPath = joinPath(path, location->index());
		if (!access(indexPath.c_str(), F_OK | R_OK) && _res.setupNormalResponse(indexPath))
			return;
		if (location->isAutoIndexable())
			return _res.setupDirectoryListing(path, _req.path());
		return _res.setupErrorResponse(FORBIDDEN, location);
	}

	if (!access(path.c_str(), F_OK | R_OK) && _res.setupNormalResponse(path))
		return;
	_res.setupErrorResponse(FORBIDDEN, location);
}

void Client::handleResponse(const sockfd &fd) {
	_res.send(fd);
	waitForCgi();
	togglePollOut();
	reset();
}

bool Client::isInternalServerError() {
	int status;
	int ret = waitpid(_pid, &status, WNOHANG);
	if (ret == _pid && ((WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status)))))
		return true;
	return false;
}

bool Client::isPurgeable(void) const {
	return !(_res.keepAlive() || (!_res.match(RES_DONE) && !_req.match(REQ_DONE)));
}

bool Client::waitForCgi() {
	if (isInternalServerError()) {
		_res.setupErrorResponse(INTERNAL_SERVER_ERROR, _ctx, true);
		_res.send(_connection.first);
		return true;
	}
	return false;
}

void Client::reset(void) {
	if (_req.match(REQ_DONE) && _res.match(RES_DONE)) {
		close(_fds[0]);
		_req.reset();
		_res.reset();
	}
}

void Client::togglePollOut(void) {
	if (_res.match(RES_BODY))
		_pfd->events |= POLLOUT;
	if (_res.match(RES_DONE | RES_INIT)) {
		(_res.match(RES_DONE)) && close(_fds[0]);
		_pfd->events &= ~POLLOUT;
	}
}

ClientMap::ClientMap() {
	_pfds = nullptr;
}

void ClientMap::changePollFds(PollFd *pfds) {
	_pfds = pfds;
}

int ClientMap::purgeConnection(const sockfd &fd) {
	iterator it = find(fd);
	if (it != end()) {
		_pfds->remove(fd);
		erase(it);
		close(fd);
		return 1;
	}
	return 0;
}

const vector<sockfd> ClientMap::getInactiveClients(void) {
	vector<sockfd> inactiveClients;
	for (iterator it = begin(); it != end(); it++)
		if (it->second.timedOut())
			inactiveClients.push_back(it->first);
	return inactiveClients;
}

int ClientMap::purgeInactiveClients() {
	int            count = 0;
	vector<sockfd> inactiveClients = getInactiveClients();

	vector<sockfd>::iterator it = inactiveClients.begin();

	while (it != inactiveClients.end())
		if (clients[*it].timedOut())
			count += clients.purgeConnection(*it++);
	return count;
}

ClientMap clients;