#include "cgi.hpp"

#include <strings.h>   // bzero (used transitively via includes elsewhere too)

using namespace std;
using servio::Result;

CGI::CGI() : _req(NULL), _res(NULL), _location(NULL) {}

Result<CGI, string> CGI::create(LocationContext<Type> *location, Request *req, Response *res) {
	typedef Result<CGI, string> R;

	CGI c;
	c._req = req;
	c._res = res;
	c._location = location;

	stringstream ss(req->path().substr(location->location().length() + 1));
	string       script;
	getline(ss, script, '/');
	getline(ss, c._pathInfo, '\0');

	c._scriptName     = joinPath(location->location(), script);
	c._scriptFileName = c._scriptName;

	CgiExtension *exts = location->cgiExtensions();
	if (!exts->match(script))
		return R::err("`" + script + "` does not match any cgi_assign extension");

	struct stat st;
	bzero(&st, sizeof st);
	if (!location->found(c._scriptFileName, st))
		return R::err("`" + c._scriptFileName + "` not found");
	if (access(c._scriptFileName.c_str(), X_OK) != 0)
		return R::err("`" + c._scriptFileName + "` is not executable");

	return R::ok(c);
}

void CGI::init() {
	metaVariables.add("GATEWAY_INTERFACE", "CGI/1.1");
	metaVariables.add("DOCUMENT_ROOT", *_location->directives()["root"].str);
	metaVariables.add("QUERY_STRING", _req->query());
	metaVariables.add("REQUEST_METHOD", httpMethods[(int)log2((int)_req->method())]);
	metaVariables.add("REQUEST_URI", _req->path() + (!_req->query().empty() ? "?" : "") + _req->query());
	metaVariables.add("SCRIPT_FILENAME", _scriptFileName);
	metaVariables.add("SCRIPT_NAME", _scriptName);
	metaVariables.add("PATH_INFO", _pathInfo);

	for (const auto &entry : _req->headers())
		metaVariables.setAll("HTTP_" + entry.first, entry.second);
}

void CGI::setenv() {
	for (const auto &entry : metaVariables)
		for (const auto &value : entry.second)
			::setenv(entry.first.c_str(), value.c_str(), 1);
}

pid_t CGI::spawn(int *fds, const int &fileno) {
	pipe(fds);
	const pid_t pid = fork();
	if (!pid) {
		lseek(fileno, 0, SEEK_SET);

		dup2(fileno, STDIN_FILENO);
		close(fds[0]);
		close(fileno);

		dup2(fds[1], STDOUT_FILENO);
		close(fds[1]);

		init();
		setenv();
		execvp(_scriptFileName.c_str(), (char *[]){NULL});
		perror("execvp");
		exit(1);
	}
	close(fds[1]);
	return pid;
}
