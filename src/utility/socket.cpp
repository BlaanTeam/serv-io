#include "socket.hpp"

#include <cerrno>
#include <cstring>

using namespace std;
using servio::Result;
using servio::Unit;

static void *getAddr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in *)sa)->sin_addr);
	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// ----------------------------------------------------------------- Address --

Address::Address() : _port(0), _ss_family(AF_INET) {}

Address::Address(const sockfd &fd) : _port(0), _ss_family(AF_INET) {
	sockaddr  sa;
	socklen_t len = sizeof(sockaddr_storage);
	if (getsockname(fd, &sa, &len) == 0)
		*this = Address(sa, len);
}

Address::Address(const sockaddr &sa, const socklen_t &len) : _port(0) {
	char buff[INET6_ADDRSTRLEN] = {0};
	_ss_family = sa.sa_family;
	inet_ntop(sa.sa_family, getAddr(const_cast<sockaddr *>(&sa)), buff, len);
	_host = string(buff);
	_port = ntohs(((const sockaddr_in *)&sa)->sin_port);
}

Address::~Address() {}

Result<Address, string> Address::parse(const string &host, int port) {
	typedef Result<Address, string> R;

	if (port < 0 || port >= (1 << 16))
		return R::err("port out of range: " + to_string(port));

	sockaddr_in sin;
	bzero(&sin, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	// Try numeric IPv4/IPv6 first; fall back to DNS resolution.
	if (inet_pton(AF_INET, host.c_str(), &sin.sin_addr) > 0)
		return R::ok(Address(*(sockaddr *)&sin, sizeof(sockaddr_in)));

	addrinfo hints;
	bzero(&hints, sizeof hints);
	hints.ai_family = AF_INET;

	addrinfo *info = NULL;
	const int rc = getaddrinfo(host.c_str(), to_string(port).c_str(), &hints, &info);
	if (rc != 0 || info == NULL)
		return R::err("could not resolve `" + host + "`: " + gai_strerror(rc));

	Address out(*info->ai_addr, info->ai_addrlen);
	freeaddrinfo(info);
	return R::ok(out);
}

ostream &operator<<(ostream &stream, const Address &addr) {
	return stream << addr.host() << ":" << addr.port();
}

string Address::host(void) const { return _host; }
int    Address::port(void) const { return _port; }

sockaddr Address::sockAddr(void) const {
	sockaddr sa;
	bzero(&sa, sizeof(sockaddr));
	sa.sa_family = _ss_family;
#ifdef __APPLE__
	// BSD-style sockaddr carries its own length; Linux's POSIX sockaddr
	// doesn't have this field.
	sa.sa_len = sockLen();
#endif

	if (_ss_family == AF_INET)
		((sockaddr_in *)&sa)->sin_port = htons(_port);
	else if (_ss_family == AF_INET6)
		((sockaddr_in6 *)&sa)->sin6_port = htons(_port);

	inet_pton(_ss_family, _host.c_str(), getAddr(&sa));
	return sa;
}

socklen_t Address::sockLen(void) const {
	return _ss_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
}

bool Address::operator<(const Address &rhs) const {
	return lexicographical_compare(_host.begin(), _host.end(),
	                               rhs._host.begin(), rhs._host.end())
	    || _port < rhs._port;
}

bool Address::operator==(const Address &rhs) const {
	return (_host == rhs._host || _host == "0.0.0.0") && _port == rhs._port;
}

// ------------------------------------------------------------------ Socket --

Socket::Socket() : _fd(-1) {}
Socket::Socket(sockfd fd) : _fd(fd) {}

Socket::~Socket() {}  // sockets are deliberately copyable for vector<Socket> use; no implicit close

sockfd Socket::fd(void) const { return _fd; }

Result<Socket, string> Socket::create(int domain, int type, int proto) {
	typedef Result<Socket, string> R;

	const sockfd fd = ::socket(domain, type, proto);
	if (fd == -1)
		return R::err(string("socket(2): ") + strerror(errno));

	const int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	return R::ok(Socket(fd));
}

Result<Unit, string> Socket::bind(const Address &addr) {
	const sockaddr  sa = addr.sockAddr();
	const socklen_t len = addr.sockLen();
	if (::bind(_fd, &sa, len) != 0)
		return Result<Unit, string>::err(string("bind(2): ") + strerror(errno));
	return Result<Unit, string>::ok(Unit());
}

Result<Unit, string> Socket::listen(int backlog) {
	if (::listen(_fd, backlog) != 0)
		return Result<Unit, string>::err(string("listen(2): ") + strerror(errno));
	return Result<Unit, string>::ok(Unit());
}

Result<pair<sockfd, Address>, string> Socket::accept() {
	typedef Result<pair<sockfd, Address>, string> R;

	sockaddr  sa;
	socklen_t sa_len = sizeof(sa);

	const sockfd peer = ::accept(_fd, &sa, &sa_len);
	if (peer == -1)
		return R::err(string("accept(2): ") + strerror(errno));

	return R::ok(make_pair(peer, Address(sa, sa_len)));
}

// ------------------------------------------------------------------ PollFd --

PollFd::PollFd() {}

class PollFd::FindPollFd {
	sockfd _fd;

   public:
	FindPollFd(const sockfd &fd) : _fd(fd) {}
	bool operator()(const pollfd &pfd) { return pfd.fd == _fd; }
};

void PollFd::add(const sockfd &fd, const short &events) {
	pollfd new_pfd = {fd, events, 0};
	push_back(new_pfd);
}

void PollFd::remove(const sockfd &fd) {
	iterator it = get(fd);
	if (it != end())
		erase(it);
}

PollFd::iterator PollFd::get(const sockfd &fd) {
	return find_if(begin(), end(), FindPollFd(fd));
}

int PollFd::poll(const int &timeout) {
	return ::poll(data(), size(), timeout);
}
