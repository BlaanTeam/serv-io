#ifndef SERVIO_AST_HPP
#define SERVIO_AST_HPP

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

#include "http/status_codes.hpp"
#include "utility/helpers.hpp"
#include "utility/socket.hpp"


enum CtxType {
	httpCtx = 1 << 1,
	serverCtx = 1 << 2,
	locationCtx = 1 << 3,
};

enum TypeList {
	INT = 1 << 0,
	STR = 1 << 1,
	BOOL = 1 << 2,
	ADDR = 1 << 3,
	ERRPG = 1 << 4,
	REDIR = 1 << 5,
	SERV_NAME = 1 << 6,
	CGI_EXT = 1 << 7,
};

struct ErrorPage {
	std::string pattern, page;
	ErrorPage(std::string pattern = "", std::string page = "");

	bool match(const int &errorCode) const;
	bool exists() const;

   private:
};

struct Redirect;

struct ServerName : public std::vector<std::string> {
	ServerName(const std::vector<std::string> &vec);
	bool find(const std::string &name);
};

struct CgiExtension : public std::vector<std::string> {
	CgiExtension(const std::vector<std::string> &vec);
	bool match(const std::string &name);
};

struct Type {
	int type;
	union {
		long long     value;
		bool          ok;
		std::string       *str;
		Address      *addr;
		ErrorPage    *errPage;
		Redirect     *redirect;
		ServerName   *servName;
		CgiExtension *cgiExt;
	};
	Type();
	Type(int type);
	Type(const Type &cpy);
	Type &operator=(const Type &cpy);
	~Type();
};

template <class T = std::vector<std::string> >
class MainContext {
   protected:
	CtxType                  _type;
	std::vector<MainContext<T> *> _contexts;
	std::map<std::string, T>           _directives;

   public:
	typedef typename std::map<std::string, T>::iterator dirIter;
	typedef std::pair<std::string, T>                   Directive;

	MainContext() {}

	MainContext(const MainContext<T> *copy) {
		_directives = copy->_directives;
	}

	CtxType type() {
		return _type;
	}

	std::map<std::string, T> &directives() {
		return _directives;
	}

	std::vector<MainContext<T> *> &contexts() {
		return _contexts;
	}

	void addContext(MainContext<T> *ctx) {
		_contexts.push_back(ctx);
	}

	void addDirective(const Directive &dir) {
		_directives[dir.first] = dir.second;
	}

	void rmDirective(const std::string &dir) {
		dirIter it = _directives.find(dir);
		if (it != _directives.end()) _directives.erase(it);
	}

	T &operator[](const std::string &dir) {
		return _directives[dir];
	}

	virtual ~MainContext() {
		for (size_t idx = 0; idx < _contexts.size(); idx++)
			delete _contexts[idx];
	};
};

template <>
class MainContext<Type> {
   protected:
	CtxType                     _type;
	std::vector<MainContext<Type> *> _contexts;
	std::map<std::string, Type>           _directives;

   public:
	typedef std::map<std::string, Type>::iterator dirIter;
	typedef std::pair<std::string, Type>          Directive;

	MainContext() {}

	MainContext(const MainContext<Type> *copy) {
		_directives = copy->_directives;
	}

	CtxType type() {
		return _type;
	}

	std::map<std::string, Type> &directives() {
		return _directives;
	}

	std::vector<MainContext<Type> *> &contexts() {
		return _contexts;
	}

	void addContext(MainContext<Type> *ctx) {
		_contexts.push_back(ctx);
	}

	void addDirective(const Directive &dir) {
		_directives[dir.first] = dir.second;
	}

	void rmDirective(const std::string &dir) {
		dirIter it = _directives.find(dir);
		if (it != _directives.end()) _directives.erase(it);
	}

	Type &operator[](const std::string &dir) {
		return _directives[dir];
	}

	bool isRedirectable() {
		return _directives.find("return") != _directives.end();
	}
	bool isCGI() {
		return _directives.find("cgi_assign") != _directives.end();
	}

	bool isUpload() {
		return _directives.find("upload_store") != _directives.end();
	}

	Redirect *redirect() {
		return _directives["return"].redirect;
	}

	CgiExtension *cgiExtensions() {
		return _directives["cgi_assign"].cgiExt;
	}

	std::string uploadStore() {
		return *_directives["upload_store"].str;
	}

	// Returns the matching error page entry, or `None` if no directive
	// covers this status code. The old `nullptr`-sentinel API is gone.
	servio::Option<ErrorPage *> errorPage(const int &errorCode) {
		for (dirIter it = _directives.begin(); it != _directives.end(); it++)
			if (it->second.type & ERRPG && it->second.errPage->match(errorCode))
				return servio::Some(it->second.errPage);
		return servio::None<ErrorPage *>();
	}

	virtual ~MainContext() {
		for (size_t idx = 0; idx < _contexts.size(); idx++)
			delete _contexts[idx];
	};
};

struct Redirect {
	int    code;
	std::string path;
	bool   isRedirect;
	bool   isLocal;
	Redirect(int code = 301, std::string path = "", bool isLocal = false);
	void prepare(MainContext<Type> *ctx);
};

template <class T = std::vector<std::string> >
class HttpContext : public MainContext<T> {
   public:
	HttpContext() {
		MainContext<T>::_type = httpCtx;
	};

	HttpContext(const MainContext<T> *copy)
	    : MainContext<T>(copy) { MainContext<T>::_type = httpCtx; }

	~HttpContext(){};
};

template <class T = std::vector<std::string> >
class ServerContext : public MainContext<T> {
   public:
	ServerContext() { MainContext<T>::_type = serverCtx; };

	ServerContext(const MainContext<T> *copy)
	    : MainContext<T>(copy) { MainContext<T>::_type = serverCtx; }

	~ServerContext(){};
};

template <class T = std::vector<std::string> >
class LocationContext : public MainContext<T> {
	std::string _loc;

   public:
	LocationContext(const std::string &loc = "")
	    : _loc(loc) { MainContext<T>::_type = locationCtx; };

	LocationContext(const MainContext<T> *copy)
	    : MainContext<T>(copy) { MainContext<T>::_type = locationCtx; }

	const std::string &location() const {
		return _loc;
	}

	void setLocation(const std::string &loc) {
		_loc = loc;
	}

	~LocationContext(){};
};
template <>
class LocationContext<Type> : public MainContext<Type> {
	std::string _loc;

   public:
	LocationContext(const std::string &loc = "")
	    : _loc(loc) { MainContext<Type>::_type = locationCtx; };

	LocationContext(const MainContext<Type> *copy)
	    : MainContext<Type>(copy) { MainContext<Type>::_type = locationCtx; }

	const std::string &location() const {
		return _loc;
	}

	void setLocation(const std::string &loc) {
		_loc = loc;
	}

	bool isAutoIndexable() {
		return _directives["autoindex"].ok;
	}

	bool isAllowedMethod(const HttpMethod &method) {
		return _directives["allowed_methods"].value & method;
	}

	bool found(std::string &path, struct stat &stat) {
		path = (*_directives["root"].str) + path;

		return ::stat(path.c_str(), &stat) == 0;
	}

	std::string index(void) {
		return *(_directives["index"].str);
	}

	~LocationContext(){};
};

template <>
class ServerContext<Type> : public MainContext<Type> {
   private:
	int commonPrefix(const std::string &s1, const std::string &s2, int start = 0) {
		int ret = start;
		while (s1[ret] && s2[ret] && s1[ret] == s2[ret])
			ret++;
		return ret;
	}
	std::pair<int, LocationContext<Type> *> search(LocationContext<Type> *tree, const std::string &path, int parentCommonPrefix = 0) {
		int currentCommonPrefix = commonPrefix(tree->location(), path, parentCommonPrefix);
		if (currentCommonPrefix != (int)tree->location().size())
			return std::make_pair(-1, nullptr);
		std::pair<int, LocationContext<Type> *> ans = std::make_pair(currentCommonPrefix, tree);
		for (size_t i = 0; i < tree->contexts().size(); i++) {
			std::pair<int, LocationContext<Type> *> p = search((LocationContext<Type> *)tree->contexts()[i], path, currentCommonPrefix);
			if (p.first > ans.first)
				ans = p;
		}
		return ans;
	}

   public:
	ServerContext() { MainContext<Type>::_type = serverCtx; };

	ServerContext(const MainContext<Type> *copy)
	    : MainContext<Type>(copy) { MainContext<Type>::_type = serverCtx; }

	LocationContext<Type> *match(const std::string &path) {
		std::pair<int, LocationContext<Type> *> ans = std::make_pair(0, nullptr);
		for (size_t i = 0; i < _contexts.size(); i++) {
			std::pair<int, LocationContext<Type> *> p = search((LocationContext<Type> *)_contexts[i], path);
			if (p.first > ans.first) {
				ans = p;
			}
		}
		return ans.second;
	}

	~ServerContext(){};
};

#endif