#include "server.hpp"

#include "utility/result.hpp"

using namespace std;
using servio::Result;
using servio::Unit;

#define RECV_BUF_SIZE (1 << 14)

static int isInSockets(const sockfd &fd, const vector<Socket> &vec) {
	for (size_t i = 0; i < vec.size(); ++i)
		if (vec[i].fd() == fd)
			return (int)i;
	return -1;
}

// Bring up one listening socket per `Address`. Any failure aborts startup
// with a single, formatted error message so callers don't have to deal with
// half-initialized state.
static Result<Unit, string> initListeningSockets(const set<Address> &addrs,
                                                 vector<Socket>     &sockets,
                                                 PollFd             &pfds) {
	for (set<Address>::const_iterator it = addrs.begin(); it != addrs.end(); ++it) {
		Result<Socket, string> made = Socket::create();
		if (made.isErr())
			return Result<Unit, string>::err(made.unwrapErr());

		Socket s = made.unwrap();

		Result<Unit, string> bound = s.bind(*it);
		if (bound.isErr())
			return Result<Unit, string>::err(bound.unwrapErr());

		Result<Unit, string> listening = s.listen();
		if (listening.isErr())
			return Result<Unit, string>::err(listening.unwrapErr());

		pfds.add(s.fd(), POLLIN);
		sockets.push_back(s);
	}
	return Result<Unit, string>::ok(Unit());
}

static set<Address> getVirtualServers(MainContext<Type> *main) {
	set<Address> addrs;
	if (!main) return addrs;
	for (size_t i = 0; i < main->contexts().size(); ++i)
		addrs.insert(*(*main->contexts()[i])["listen"].addr);
	return addrs;
}

static void handleConnection(PollFd::iterator   &it,
                             PollFd             &tmp,
                             vector<Socket>     &sockets,
                             char               *recvBuf) {
	const int idx = isInSockets(it->fd, sockets);

	if (idx != -1 && (it->revents & POLLIN)) {
		Result<pair<sockfd, Address>, string> r = sockets[idx].accept();
		if (r.isErr()) return;  // transient kernel error; leave the listener registered
		pair<sockfd, Address> conn = r.unwrap();
		tmp.add(conn.first, POLLIN);
		clients[conn.first] = Client(conn);
		return;
	}

	if (it->revents & POLLIN) {
		const int nbyte = recv(it->fd, recvBuf, RECV_BUF_SIZE, 0);
		if (nbyte > 0) {
			clients[it->fd].setTime(getmstime());
			clients[it->fd].setPollFd(it->fd, tmp);
			if (clients[it->fd].handleRequest(recvBuf, (size_t)nbyte))
				clients.purgeConnection(it->fd);
			return;
		}
		if (nbyte == 0) {
			clients.purgeConnection(it->fd);
			return;
		}
	}

	if (it->revents & POLLOUT) {
		clients[it->fd].setPollFd(it->fd, tmp);
		clients[it->fd].handleResponse(it->fd);
	}

	if (it->revents & POLLHUP)
		clients.purgeConnection(it->fd);
}

// Run the accept/poll loop. Returns Ok(()) on a clean shutdown (currently
// not reachable — the loop is infinite) or Err(msg) on startup failure.
Result<Unit, string> servio_run(const int &ac, char *const *av) {
	if (!parse_options(ac, av, config))
		return Result<Unit, string>::ok(Unit());   // -h/-v style early-exit

	Result<Unit, string> parsed = config.parse();
	if (parsed.isErr())
		return parsed;

	const set<Address> addrs = getVirtualServers(config.ast());

	vector<Socket> sockets;
	PollFd         pfds;
	sockets.reserve(addrs.size());

	Result<Unit, string> init = initListeningSockets(addrs, sockets, pfds);
	if (init.isErr()) return init;

	char recvBuf[RECV_BUF_SIZE];

	while (true) {
		if (pfds.poll(TIMEOUT) == -1)
			perror("poll");

		clients.changePollFds(&pfds);
		clients.purgeInactiveClients();

		PollFd tmp = pfds;
		clients.changePollFds(&tmp);

		for (PollFd::iterator it = pfds.begin(); it != pfds.end(); ++it)
			handleConnection(it, tmp, sockets, recvBuf);

		pfds = tmp;
	}
}

void handleSignals(void) {
	signal(SIGPIPE, SIG_IGN);
}
