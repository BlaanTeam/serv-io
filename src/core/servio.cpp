#include "servio.hpp"

static int isInSockets(const sockfd &fd, const vector<Socket> &vec) {
	vector<Socket>::const_iterator it = vec.begin();

	while (it != vec.end()) {
		if (fd == it->getSockFd())
			return distance(vec.begin(), it);
		it++;
	}
	return -1;
}

static void initListeningSockets(const set<Address> &addrs, vector<Socket> &sockets, PollFd &pfds) {
	set<Address>::const_iterator it = addrs.begin();

	while (it != addrs.end()) {
		Socket socket;

		if (!socket.isGood()) {
			perror("socket");
			exit(1);
		}

		socket.bind(*it);
		if (!socket.isGood()) {
			perror("bind");
			exit(1);
		}

		socket.listen();
		if (!socket.isGood()) {
			perror("listen");
			exit(1);
		}

		pfds.add(socket.getSockFd(), POLLIN);
		sockets.push_back(socket);
		it++;
	}
}

static set<Address> getVirtualServers(MainContext<Type> *main) {
	set<Address> addrs;

	if (!main)
		return addrs;

	for (size_t idx = 0; idx < main->contexts().size(); idx++)
		addrs.insert(*(*main->contexts()[idx])["listen"].addr);
	return addrs;
}

#define RECV_BUF_SIZE (1 << 14)

void servio_init(const int &ac, char *const *av) {
	char stream[RECV_BUF_SIZE];

	if (!parse_options(ac, av, config) || !config.syntaxOnly())
		return;

	set<Address> addrs = getVirtualServers(config.ast());

	vector<Socket> sockets;
	PollFd         pfds;

	sockets.reserve(addrs.size());
	initListeningSockets(addrs, sockets, pfds);

	while (true) {
		int ret = pfds.poll(TIMEOUT);
		if (ret == -1)
			perror("poll");

		clients.changePollFds(&pfds);
		clients.purgeInactiveClients();

		PollFd::iterator it = pfds.begin();
		PollFd           tmp = pfds;

		while (it != pfds.end()) {
			clients.changePollFds(&tmp);
			const int idx = isInSockets(it->fd, sockets);
			if (idx != -1 && it->revents & POLLIN) {
				pair<int, Address> newConnection = sockets[idx].accept();
				// cerr << "New Connection From " << newConnection.second << endl;
				tmp.add(newConnection.first, POLLIN);
				clients[newConnection.first] = Client(newConnection);
			} else if (it->revents & POLLIN) {
				int nbyte = recv(it->fd, stream, RECV_BUF_SIZE, 0);
				if (nbyte > 0) {
					clients[it->fd].setTime(getmstime());
					clients[it->fd].setPollFd(it->fd, tmp);
					if (clients[it->fd].handleRequest(stream, (size_t)nbyte))
						goto purgeConnection;
				} else if (nbyte == 0)
					goto purgeConnection;
			}
			if (it->revents & POLLOUT) {
				// cerr << "POLLOUT" << endl;
				clients[it->fd].setPollFd(it->fd, tmp);
				clients[it->fd].handleResponse(it->fd);
			}
			if (it->revents & POLLHUP) {
			purgeConnection:
				// cerr << "Client Disconnected !" << endl;
				clients.purgeConnection(it->fd);
			}
			it++;
		}
		pfds = tmp;
	}
}

void handleSignals(void) {
	signal(SIGPIPE, SIG_IGN);
}