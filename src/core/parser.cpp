#include "parser.hpp"

// Public member functions

Parser::Parser(ifstream &cfile) {
	if (!_lex.tokenizer(cfile))
		_serr = "LINE " + to_string(_lex.back().line()) + ": found EOF while looking for matching `\"'";
};

MainContext<Type> *Parser::parse() {
	if (!_serr.empty())
		return nullptr;

	MainContext<> *tree = parse_main();

	if (!updateDirectives(tree))
		return delete tree, nullptr;

	pair<bool, MainContext<Type> *> ret = transfer(tree);

	delete tree;

	if (!ret.first)
		return delete ret.second, nullptr;

	return ret.second;
};

const string &Parser::err() const {
	return _serr;
}

// Parsing utils

const Token &Parser::current() {
	return _lex.front();
}

bool Parser::accept(int type, const string &value) {
	if ((current().type() & type) && (value.empty() || current().value() == value))
		return _lex.pop_front(), true;
	return false;
}

bool Parser::expect(int type, const string &value) {
	if (!accept(type, value)) {
		if (_serr.empty())
			_serr = "LINE " + to_string(current().line()) + ": expected token <" + name(type) + (value[0] ? ":" + value + ">" : ">") + ", but found \"" + current().value() + "\" !";
		return false;
	}
	return true;
}

// Top-Down Parser

Parser::Directive *Parser::parse_directive(Parser::Directive *_dir) {
	if (_dir)
		return _dir;

	Parser::Directive *dir = new Directive();

	dir->first = current().value();

	if (!expect(WORD))
		goto failed;

	while (current().type() & WORD) {
		dir->second.push_back(current().value());
		_lex.pop_front();
	}

	if (!expect(SEMICOLON))
		goto failed;

	return dir;

failed:
	return delete dir, nullptr;
}

Parser::Directive *Parser::parse_http_dir(Parser::Directive *_dir) {
	Parser::Directive *dir = parse_directive(_dir);

	if (!dir)
		return nullptr;

	if (dir->first == "root" || dir->first == "index") {
		if (dir->second.size() != 1) {
			_serr = dir->first + " directive: invalid arguments!";
			goto failed;
		}
		if (dir->first == "index" && dir->second.front().empty()) {
			_serr = "index directive: invlid arguments!";
			goto failed;
		}
		return dir;
	}

	if (dir->first == "autoindex") {
		if (dir->second.size() != 1 || !(dir->second[0] == "on" || dir->second[0] == "off")) {
			_serr = "autoindex directive: invalid arguments!";
			goto failed;
		}
		return dir;
	}

	if (dir->first == "client_max_body_size") {
		if (dir->second.size() != 1 || dir->second.front().empty()) {
			_serr = "client_max_body_size directive: invalid arguments!";
			goto failed;
		}
		int    value = 0;
		size_t last;
		stoi(dir->second[0], &last);
		if (!isdigit(dir->second[0][0]) || value < 0 || last < (dir->second[0].size() - 1)) {
			_serr = "client_max_body_size directive: invalid arguments!";
			goto failed;
		} else if (last == (dir->second[0].size() - 1) && !strchr("gmkb", dir->second[0][last])) {
			_serr = "client_max_body_size directive: invalid arguments!";
			goto failed;
		}
		return dir;
	}

	if (dir->first == "error_page") {
		if (dir->second.size() != 2 || dir->second[0].size() > 3) {
			_serr = "error_page directive: invalid arguments!";
			goto failed;
		}
		for (size_t i = 0; i < dir->second[0].size(); i++) {
			if (!isdigit(dir->second[0][i]) && (dir->second[0][i] != 'x')) {
				_serr = "error_page directive: invalid arguments!";
				goto failed;
			}
		}
		dir->first += "_" + dir->second[0];  // to differentiate between multiple error_page keys
		return dir;
	}

	if (dir->first == "allowed_methods") {
		if (dir->second.empty()) {
			_serr = "allowed_methods directive: invalid arguments!";
			goto failed;
		}
		for (size_t i = 0; i < dir->second.size(); i++) {
			if (dir->second[i] != "GET" && dir->second[i] != "POST" && dir->second[i] != "DELETE") {
				_serr = "allowed_methods directive: invalid arguments!";
				goto failed;
			}
		}
		return dir;
	}

	_serr = "http context: \"" + dir->first + "\": invalid directive!";

failed:
	return delete dir, nullptr;
}

Parser::Directive *Parser::parse_server_dir() {
	Parser::Directive *dir = parse_directive();

	if (!dir)
		return nullptr;

	string directiveName = dir->first;

	if (directiveName == "listen") {
		if (dir->second.size() != 1) {
			_serr = "listen directive: invalid arguments!";
			goto failed;
		}
		return dir;
	}

	if (directiveName == "server_name") {
		if (dir->second.empty() || dir->second.size() > 512) {  // http://nginx.org/en/docs/http/ngx_http_core_module.html#server_names_hash_max_size
			_serr = "server_name: invalid arguments!";
			goto failed;
		}
		for (size_t i = 0; i < dir->second.size(); i++) {
			if (dir->second[i].size() > 64) {  // http://nginx.org/en/docs/http/ngx_http_core_module.html#server_names_hash_bucket_size
				_serr = "server_name: \"" + dir->second[i] + "\": invlid argument!";
				goto failed;
			}
		}
		return dir;
	}

	if (directiveName == "return") {
		if (dir->second.empty() || dir->second.size() > 2) {
			_serr = "return directive: invalid arguments!";
			goto failed;
		}
		size_t sz;
		stoi(dir->second[0], &sz);
		if (sz != dir->second[0].size()) {  // is integer
			_serr = "return directive: invalid arguments!";
			goto failed;
		}
		return dir;
	}

	if (parse_http_dir(dir)) {
		return dir;
	}

	if (_serr.substr(0, 4) == "http") {
		_serr = "server context: \"" + directiveName + "\": invalid directive!";
	}

	return nullptr;

failed:
	return delete dir, nullptr;
}

Parser::Directive *Parser::parse_location_dir() {
	Parser::Directive *dir = parse_directive();

	if (!dir)
		return nullptr;

	string directiveName = dir->first;

	if (directiveName == "return") {
		if (dir->second.empty() || dir->second.size() > 2) {
			_serr = "return directive: invalid arguments!";
			goto failed;
		}
		size_t sz;
		stoi(dir->second[0], &sz);
		if (sz != dir->second[0].size()) {  // is integer
			_serr = "return directive: invalid arguments!";
			goto failed;
		}
		return dir;
	}

	if (directiveName == "upload_store") {
		if (dir->second.size() != 1 || dir->second.front().empty()) {
			_serr = "upload_store directive: invalid arguments!";
			goto failed;
		}
		return dir;
	}

	if (directiveName == "cgi_assign") {
		if (dir->second.empty() || dir->second.size() > 512) {
			_serr = "cgi_assign: invalid arguments!";
			goto failed;
		}
		return dir;
	}

	if (parse_http_dir(dir)) {
		return dir;
	}

	if (_serr.substr(0, 4) == "http") {
		_serr = "location context: \"" + directiveName + "\": invalid directive!";
	}

	return nullptr;

failed:
	return delete dir, nullptr;
}

MainContext<> *Parser::parse_location() {
	MainContext<> *ret = new LocationContext<>();

	if (!expect(WORD, "location"))
		goto failed;

	((LocationContext<> *)ret)->setLocation(current().value());

	if (!expect(WORD) || !expect(OCURLY))
		goto failed;

	while (current().type() & ~CCURLY) {
		if (current().value() == "location") {
			MainContext<> *p = parse_location();
			if (!p)
				goto failed;
			ret->addContext(p);
		} else {
			Parser::Directive *dir = parse_location_dir();
			if (!dir)
				goto failed;
			ret->addDirective(*dir);
			delete dir;
		}
	}

	if (!expect(CCURLY))
		goto failed;

	return ret;

failed:
	return delete ret, nullptr;
}

MainContext<> *Parser::parse_server() {
	MainContext<> *ret = new ServerContext<>();

	if (!expect(WORD, "server") || !expect(OCURLY))
		goto failed;

	(*ret)["listen"].push_back("0.0.0.0:8080");
	(*ret)["server_name"];

	while (current().type() & ~CCURLY) {
		if (current().value() == "location") {
			MainContext<> *p = parse_location();
			if (!p)
				goto failed;
			ret->addContext(p);
		} else {
			Parser::Directive *dir = parse_server_dir();
			if (!dir)
				goto failed;
			ret->addDirective(*dir);
			delete dir;
		}
	}

	if (!expect(CCURLY))
		goto failed;

	return ret;

failed:
	return delete ret, nullptr;
}

MainContext<> *Parser::parse_main() {
	if (current().type() == _EOF) {
		_serr = "empty config";
		return nullptr;
	}

	MainContext<> *ret = new HttpContext<>();

	if (!expect(WORD, "http") || !expect(OCURLY))
		goto failed;

	(*ret)["root"].push_back("html");
	(*ret)["autoindex"].push_back("off");
	(*ret)["index"].push_back("index.html");
	(*ret)["allowed_methods"].push_back("GET");
	(*ret)["allowed_methods"].push_back("POST");
	(*ret)["allowed_methods"].push_back("DELETE");
	(*ret)["client_max_body_size"].push_back("1048576");

	while (current().type() & ~CCURLY) {
		if (current().value() == "server") {
			MainContext<> *p = parse_server();
			if (!p)
				goto failed;
			ret->addContext(p);
		} else {
			Parser::Directive *dir = parse_http_dir();
			if (!dir)
				goto failed;
			ret->addDirective(*dir);
			delete dir;
		}
	}

	if (!expect(CCURLY) || !expect(_EOF))
		goto failed;

	return ret;

failed:
	return delete ret, nullptr;
}

static void updateRootDirective(string &value) {
	if (value.length() > 0 && value[0] != '/')
		value = joinPath(PREFIX_FOLDER, value);
}

static bool inheritable(const string &dirName) {
	return !(dirName == "return" || dirName == "cgi_assign" || dirName == "upload_store");
}

bool Parser::updateDirectives(MainContext<> *tree, MainContext<> *parent) {
	if (!tree)
		return false;

	if ((tree->type() & locationCtx) && (parent->type() & locationCtx)) {
		const string &pre = ((LocationContext<> *)parent)->location();
		const string &str = ((LocationContext<> *)tree)->location();
		if (str.substr(0, pre.size()) != pre)
			return _serr = "location \"" + str + "\" is outside location \"" + pre + "\"", false;
	}

	if (parent) {
		map<string, vector<string> > &directives = parent->directives();
		for (MainContext<>::dirIter dir = directives.begin(); dir != directives.end(); dir++) {
			if (inheritable(dir->first) && tree->directives().find(dir->first) == tree->directives().end()) {
				tree->directives().insert(*dir);
			}
		}
	}

	bool has_slash_location = 0;

	for (size_t i = 0; i < tree->contexts().size(); i++) {
		if (tree->type() == serverCtx && ((LocationContext<> *)tree->contexts()[i])->location() == "/")
			has_slash_location = 1;
		if (!updateDirectives(tree->contexts()[i], tree))
			return false;
	}

	if (tree->type() == serverCtx && !has_slash_location) {
		tree->contexts().push_back(new LocationContext<>("/"));
		if (!updateDirectives(tree->contexts().back(), tree))
			return false;
	}

	return true;
}

pair<bool, MainContext<Type> *> Parser::transfer(MainContext<> *tree) {
	if (!tree)
		return make_pair(false, nullptr);

	MainContext<Type> *ret = tree->type() & httpCtx     ? (MainContext<Type> *)new HttpContext<Type>()
	                         : tree->type() & serverCtx ? (MainContext<Type> *)new ServerContext<Type>()
	                                                    : (MainContext<Type> *)new LocationContext<Type>(((LocationContext<> *)tree)->location());

	updateRootDirective((*tree)["root"][0]);

	MainContext<>::dirIter it = tree->directives().begin();

	while (it != tree->directives().end()) {
		const string         &key = it->first;
		const vector<string> &val = it->second;
		Type                  typ;

		if (key == "allowed_methods") {
			typ.type = INT;
			typ.value = 0;

			for (size_t i = 0; i < val.size(); i++) {
				if (val[i] == "GET")
					typ.value |= GET;
				else if (val[i] == "POST")
					typ.value |= POST;
				else /*DELETE*/
					typ.value |= DELETE;
			}
		}

		else if (key == "client_max_body_size") {
			typ.type = INT;
			typ.value = stol(val[0]);

			if (val[0].back() == 'k')
				typ.value *= (1LL << 10);
			else if (val[0].back() == 'm')
				typ.value *= (1LL << 20);
			else if (val[0].back() == 'g')
				typ.value *= (1LL << 30);
		}

		else if (key == "autoindex") {
			typ.type = BOOL;
			typ.ok = (val[0] == "on");
		}

		else if (key.substr(0, 10) == "error_page") {
			typ.type = ERRPG;
			typ.errPage = new ErrorPage(val[0], !val[1].empty() && val[1][0] == '/' ? val[1] : joinPath((*tree)["root"][0], val[1]));
		}

		else if (key == "return") {
			typ.type = REDIR;
			typ.redirect = new Redirect(stoi(val[0]), val.size() == 2 ? val[1] : "");
		}

		else if (key == "listen") {
			typ.type = ADDR;
			const string &addr = val[0];
			int           idx = addr.size() - 1;
			int           port = 8080;
			string        host = "0.0.0.0";

			while (idx >= 0 && isdigit(addr[idx])) {
				idx--;
			}

			if (idx == -1 || addr[idx] == ':') {
				if (idx + 1 != (int)addr.size()) port = stoi(addr.substr(idx + 1));
				if (--idx >= 0) host = addr.substr(0, idx + 1);
			} else {
				host = addr;
			}

			typ.addr = new Address(host, port);

			if (!typ.addr->good()) {
				return _serr = "invalid adress: " + addr, make_pair(false, ret);
			}
		}

		else if (key == "server_name") {
			typ.type = SERV_NAME;
			typ.servName = new ServerName(val);
		}

		else if (key == "cgi_assign") {
			typ.type = CGI_EXT;
			typ.cgiExt = new CgiExtension(val);
		}

		else if (key == "upload_store") {
			typ.type = STR;
			typ.str = new string(!val[0].empty() && val[0][0] == '/'? val[0]: joinPath(PREFIX_FOLDER, val[0]));
		}

		else {  // index, root
			typ.type = STR;
			typ.str = new string(val[0]);
		}

		(*ret)[key] = typ;

		it++;
	}

	for (size_t i = 0; i < tree->contexts().size(); i++) {
		pair<bool, MainContext<Type> *> child = transfer(tree->contexts()[i]);
		ret->addContext(child.second);
		if (!child.first)
			return make_pair(false, ret);
	}

	return make_pair(true, ret);
}