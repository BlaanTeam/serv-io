#ifndef SERVIO_CGI_HPP
#define SERVIO_CGI_HPP

#include <map>
#include <sstream>
#include <string>

#include "./header.hpp"
#include "./request.hpp"
#include "./response.hpp"
#include "core/ast.hpp"
#include "utility/result.hpp"

// CGI/1.1 fork+exec helper. Instances are only constructed through
// `CGI::create()`, which validates that the request path resolves to a
// script under one of the location's configured cgi_assign extensions and
// that the script is executable. Returns Err if any of those checks fail.
class CGI {
   public:
	std::string _scriptFileName;

	static servio::Result<CGI, std::string> create(LocationContext<Type> *location,
	                                               Request               *req,
	                                               Response              *res);

	CGI();   // empty placeholder — Result needs default-constructibility

	void  init();
	void  setenv();
	pid_t spawn(int *fds, const int &fileno);

   private:
	Request               *_req;
	Response              *_res;
	LocationContext<Type> *_location;

	std::string _scriptName;
	std::string _pathInfo;

	Header metaVariables;
};

#endif
