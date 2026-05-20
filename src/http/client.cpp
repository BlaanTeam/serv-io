#include "client.hpp"

using namespace std;

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

// Resolves the response by handing the request to the routing pipeline.
// Every branch is encapsulated in a Handler (see router.cpp); this method
// is just the glue.
void Client::resolveResponse(VirtualServer *virtualServer) {
	DefaultRouter  router;
	RoutingContext ctx(_req, _res, virtualServer, _fds, &_pid);
	router.route(ctx);
	if (ctx.location)
		_ctx = ctx.location;
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