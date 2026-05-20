#ifndef SERVIO_CLIENT_HPP
#define SERVIO_CLIENT_HPP

#include <unistd.h>

#include <fstream>
#include <iostream>
#include <map>

#include "./cgi.hpp"
#include "./request.hpp"
#include "./response.hpp"
#include "core/config.hpp"

using namespace std;

class Client {
	pollfd               *_pfd;
	pair<sockfd, Address> _connection;
	long long             _time;
	Request               _req;
	Response              _res;

	MainContext<Type> *_ctx;

	int   _fds[2];
	pid_t _pid;

   public:
	Client();
	Client(const pair<sockfd, Address> &connection);
	~Client();

	// Setters
	void setTime(const long long &time);
	void setPollFd(const sockfd &fd, PollFd &pfd);

	// Getters
	bool timedOut() const;

	bool handleRequest(const char *buf, size_t len);

	void handleResponse(const sockfd &fd);

	bool isInternalServerError();

	bool isPurgeable() const;

	bool waitForCgi();

	void reset(void);

	void togglePollOut(void);

   private:
	// `handleRequest` is split into a tiny top-level state machine and a
	// router that decides which Response to install. Both have a single
	// exit path; no goto.
	void resolveResponse(VirtualServer *virtualServer);
	bool tryCGI(Location *location);
	void resolveStaticFile(Location *location, string path);
};

class ClientMap : public map<sockfd, Client> {
	PollFd *_pfds;

	const vector<sockfd> getInactiveClients(void);

   public:
	ClientMap();

	void changePollFds(PollFd *pfds);

	int purgeConnection(const sockfd &fd);

	int purgeInactiveClients(void);
};

extern ClientMap clients;

#endif