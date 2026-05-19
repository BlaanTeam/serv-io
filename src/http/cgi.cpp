#include "cgi.hpp"

CGI::CGI(LocationContext<Type> *location, Request *req, Response *res) : _isCGI(false) {
	_location = location;
	_req = req;
	_res = res;
	stringstream ss(req->getPath().substr(location->location().length() + 1));  // ! GOT ME : 1zx0
	string       tmp;
	string       scriptFileName;

	getline(ss, tmp, '/');
	getline(ss, _pathInfo, '\0');

	_scriptName = joinPath(location->location(), tmp);
	_scriptFileName = _scriptName;

	struct stat fileStat;
	bzero(&fileStat, sizeof fileStat);

	CgiExtension *cgiExt = location->getCGIExtensions();

	if (cgiExt->match(tmp) && location->found(_scriptFileName, fileStat)) {
		if (!access(_scriptFileName.c_str(), X_OK))
			_isCGI = true;
	}
}

bool CGI::valid() const {
	return _isCGI;
}

void CGI::init() {
	metaVariables.add("GATEWAY_INTERFACE", "CGI/1.1");
	metaVariables.add("DOCUMENT_ROOT", *_location->directives()["root"].str);
	metaVariables.add("QUERY_STRING", _req->getQuery());
	metaVariables.add("REQUEST_METHOD", httpMethods[(int)log2((int)_req->getMethod())]);
	metaVariables.add("REQUEST_URI", _req->getPath() + (!_req->getQuery().empty() ? "?" : "") + _req->getQuery());
	metaVariables.add("SCRIPT_FILENAME", _scriptFileName);
	metaVariables.add("SCRIPT_NAME", _scriptName);
	metaVariables.add("PATH_INFO", _pathInfo);

	Request::headerIter it = _req->getHeaders().begin();

	while (it != _req->getHeaders().end()) {
		metaVariables["HTTP_" + it->first] = it->second;
		it++;
	}
}

void CGI::setenv() {
	Header::iterator it = metaVariables.begin();
	while (it != metaVariables.end()) {
		for (set<string>::iterator it_ = it->second.begin(); it_ != it->second.end(); it_++)
			::setenv(it->first.c_str(), it_->c_str(), 1);

		it++;
	}
}

pid_t CGI::spawn(int *fds, const int &fileno) {
	pipe(fds);
	int pid = fork();
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