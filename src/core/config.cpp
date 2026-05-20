#include "config.hpp"

#include <cerrno>
#include <cstring>

using namespace std;
using servio::Result;
using servio::Unit;

Config config;

Config::Config(const string &path) {
	(void)load(path);  // failures stay in the file_stream state; surface
	                   // through parse()'s Result on next call
}

Config::~Config() {
	_file_stream.close();
	// _asTree cleaned up automatically by unique_ptr.
}

Result<Unit, string> Config::load(const string &path) {
	_file_stream.close();
	_file_stream.clear();
	_file_stream.open(path.c_str(), ios::in);
	_path = path;
	if (!_file_stream.good())
		return Result<Unit, string>::err("could not open `" + path + "`: " + strerror(errno));
	return Result<Unit, string>::ok(Unit());
}

Result<Unit, string> Config::parse() {
	if (!_file_stream.good())
		return Result<Unit, string>::err("config file `" + _path + "` is not open");

	Parser parser(_file_stream);
	MainContext<Type> *tree = parser.parse();
	if (!tree)
		return Result<Unit, string>::err(parser.err());

	_asTree.reset(tree);
	return Result<Unit, string>::ok(Unit());
}

void Config::displayContent(void) const {
	char buff[1 << 10];
	while (_file_stream.good() && !_file_stream.eof()) {
		buff[_file_stream.read(buff, (1 << 10) - 1).gcount()] = 0x0;
		cout << buff;
	}
}

MainContext<Type> *Config::ast()              { return _asTree.get(); }
string             Config::path(void) const { return _path; }

VirtualServer *Config::match(const Address &addr, const string &host) {
	vector<VirtualServer *> servers;

	for (size_t i = 0; i < _asTree->contexts().size(); ++i)
		if (*(*(VirtualServer *)_asTree->contexts()[i])["listen"].addr == addr)
			servers.push_back((VirtualServer *)_asTree->contexts()[i]);

	for (size_t i = 0; i < servers.size(); ++i)
		if ((*servers[i])["server_name"].servName->find(host))
			return servers[i];

	return servers.empty() ? NULL : servers[0];
}
