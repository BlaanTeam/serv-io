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

bool Client::handleRequest(const char *buf, size_t len) {
	reset();
	_req.consume(buf, len);

	VirtualServer *virtualServer = config.match(Address(_connection.first), _req.getHeaders().get("Host"));

	_ctx = virtualServer;

	if (!_req.valid()) {
		_res.setupErrorResponse(_req.getStatusCode(), virtualServer);
		goto sendResponse;
	} else if (_req.match(REQ_BODY | REQ_DONE)) {
		if (_res.match(RES_INIT)) {
			Location *location = virtualServer->match(_req.getPath());
			string    path = _req.getPath();

			_ctx = location;
			if (virtualServer->isRedirectable()) {
				_res.setupRedirectResponse(virtualServer->getRedir(), virtualServer);
				goto sendResponse;
			} else if (!location) {
				_res.setupErrorResponse(NOT_FOUND, virtualServer);
				goto sendResponse;
			} else if (_req.isTooLarge(location->directives()["client_max_body_size"].value)) {
				_res.setupErrorResponse(REQUEST_ENTITY_TOO_LARGE, virtualServer);
				goto sendResponse;
			} else if (location->isRedirectable()) {
				_res.setupRedirectResponse(location->getRedir(), location);
				goto sendResponse;
			} else if (!location->isAllowedMethod(_req.getMethod())) {
				_res.setupErrorResponse(METHOD_NOT_ALLOWED, location);
				goto sendResponse;
			}

			size_t locationLength = location->location().length();
			size_t pathLength = _req.getPath().length();

			if (location->isCGI() && pathLength > locationLength) {
				CGI cgi(location, &_req, &_res);
				if (cgi.valid()) {
					_pid = cgi.spawn(_fds, _req.getFileno());
					_res.setupCGIResponse(_fds[0], &_req);
					goto sendResponse;
				}
			}
			if (location->isUpload() && _req.getMethod() == POST) {
				_res.setupUploadResponse(location, &_req);
				goto sendResponse;
			}

			_res.extractRange(_req);
			struct stat fileStat;
			bzero(&fileStat, sizeof fileStat);
			if (!location->found(path, fileStat)) {
				_res.setupErrorResponse(NOT_FOUND, location);
			} else if (S_ISDIR(fileStat.st_mode)) {
				// Redirect directory requests that lack a trailing slash.
				if (pathLength > 1 && _req.getPath()[pathLength - 1] != '/') {
					Redirect redir(MOVED_PERMANENTLY, joinPath(_req.getPath(), "/"), true);
					_res.setupRedirectResponse(&redir, location);
					goto sendResponse;
				}

				string tmp = joinPath(path, location->getIndex());
				if (!access(tmp.c_str(), F_OK | R_OK) && _res.setupNormalResponse(tmp))
					;
				else if (location->isAutoIndexable())
					_res.setupDirectoryListing(path, _req.getPath());
				else
					_res.setupErrorResponse(FORBIDDEN, location);
			} else {
				if (!access(path.c_str(), F_OK | R_OK) && _res.setupNormalResponse(path))
					;
				else
					_res.setupErrorResponse(FORBIDDEN, location);
			}
		}
	sendResponse:
		_req.closeBodyFile();
		_res.send(_connection.first);

		if (waitForCgi()) return false;

		if (!_res.match(RES_DONE)) setTime(getmstime());
	}

	togglePollOut();

	return isPurgeable();
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