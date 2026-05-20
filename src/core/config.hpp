#ifndef SERVIO_CONFIG_HPP
#define SERVIO_CONFIG_HPP

#include <fstream>
#include <string>

#include "./ast.hpp"
#include "./parser.hpp"
#include "http/status_codes.hpp"
#include "utility/result.hpp"
#include "utility/utils.hpp"

using namespace std;

#define CONF_DFL_PATH "conf/servio.conf"

typedef LocationContext<Type> Location;
typedef ServerContext<Type>   VirtualServer;

class Config {
   public:
	Config(const string &path = CONF_DFL_PATH);
	~Config();

	// Open the given path and parse the configuration. Errors are surfaced
	// via Result instead of cerr; callers decide how / whether to display
	// them.
	servio::Result<servio::Unit, string> load(const string &path);

	// Parses the previously-loaded file. Idempotent in the sense that it
	// always returns the same Result; the parsed AST is owned by Config.
	servio::Result<servio::Unit, string> parse();

	void displayContent(void) const;

	// Getters
	MainContext<Type> *ast();
	string             getPath(void) const;
	VirtualServer     *match(const Address &addr, const string &host);

   private:
	string             _path;
	mutable ifstream   _file_stream;
	MainContext<Type> *_asTree;
};

extern Config config;

#endif
