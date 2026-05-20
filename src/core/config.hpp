#ifndef SERVIO_CONFIG_HPP
#define SERVIO_CONFIG_HPP

#include <fstream>
#include <memory>
#include <string>

#include "./ast.hpp"
#include "./parser.hpp"
#include "http/status_codes.hpp"
#include "utility/result.hpp"
#include "utility/utils.hpp"

#define CONF_DFL_PATH "conf/servio.conf"

typedef LocationContext<Type> Location;
typedef ServerContext<Type>   VirtualServer;

class Config {
   public:
	Config(const std::string &path = CONF_DFL_PATH);
	~Config();

	// Open the given path and parse the configuration. Errors are surfaced
	// via Result instead of cerr; callers decide how / whether to display
	// them.
	servio::Result<servio::Unit, std::string> load(const std::string &path);

	// Parses the previously-loaded file. Idempotent in the sense that it
	// always returns the same Result; the parsed AST is owned by Config.
	servio::Result<servio::Unit, std::string> parse();

	void displayContent(void) const;

	// Getters
	MainContext<Type> *ast();
	std::string        path(void) const;
	VirtualServer     *match(const Address &addr, const std::string &host);

   private:
	std::string                          _path;
	mutable std::ifstream                _file_stream;
	std::unique_ptr<MainContext<Type> >  _asTree;
};

extern Config config;

#endif
