#ifndef SERVIO_SOCKET_HPP
#define SERVIO_SOCKET_HPP

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "result.hpp"

typedef int sockfd;

// `listen(2)` backlog used by `Socket::listen()`.
static const int BACKLOG = 10;

using namespace std;

// Network address (host + port). Construction never fails — fallible input
// goes through the static `parse()` factory and surfaces as a Result.
class Address {
   public:
	static servio::Result<Address, string> parse(const string &host, int port);

	Address();                                       // empty placeholder
	Address(const sockfd &fd);                       // from an accepted fd
	Address(const sockaddr &addr, const socklen_t &len);

	bool operator<(const Address &rhs)  const;
	bool operator==(const Address &rhs) const;

	string    host(void) const;
	int       port(void) const;
	sockaddr  sockAddr(void) const;
	socklen_t sockLen(void) const;

	~Address();

   private:
	string       _host;
	unsigned int _port;
	short        _ss_family;
};

ostream &operator<<(ostream &stream, const Address &addr);

// Listening / accepted TCP socket. Construction goes through `create()`, so
// callers handle the socket(2) failure case explicitly via Result.
class Socket {
   public:
	static servio::Result<Socket, string> create(int domain = AF_INET,
	                                             int type   = SOCK_STREAM,
	                                             int proto  = 0);

	servio::Result<servio::Unit, string>          bind(const Address &addr);
	servio::Result<servio::Unit, string>          listen(int backlog = BACKLOG);
	servio::Result<pair<sockfd, Address>, string> accept();

	sockfd fd(void) const;

	Socket();                       // empty placeholder — Result needs default-constructibility
	explicit Socket(sockfd fd);
	~Socket();

   private:
	sockfd _fd;
};

class PollFd : public vector<pollfd> {
	class FindPollFd;

   public:
	PollFd();

	void add(const sockfd &fd, const short &events);
	void remove(const sockfd &fd);
	int  poll(const int &timeout);

	iterator get(const sockfd &fd);
};

#endif
