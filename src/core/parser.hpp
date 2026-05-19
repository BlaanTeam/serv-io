#ifndef __PARSER_H__
#define __PARSER_H__

#include "ast.hpp"
#include "status_codes.hpp"
#include "lexer.hpp"

#ifndef PREFIX_FOLDER
#define PREFIX_FOLDER ""
#endif

class Parser {
	string _serr;
	Lexer  _lex;

	typedef pair<string, vector<string> > Directive;

   private:
	const Token &current();
	bool         accept(int type, const string &value = "");
	bool         expect(int type, const string &value = "");
	bool         updateDirectives(MainContext<vector<string> > *tree,
	                              MainContext<vector<string> > *parent = nullptr);

	Directive *parse_directive(Directive *_dir = nullptr);
	Directive *parse_http_dir(Directive *_dir = nullptr);
	Directive *parse_server_dir();
	Directive *parse_location_dir();

	MainContext<vector<string> > *parse_location();
	MainContext<vector<string> > *parse_server();
	MainContext<vector<string> > *parse_main();

	pair<bool, MainContext<Type> *> transfer(MainContext<vector<string> > *tree);

   public:
	Parser(ifstream &cfile);
	MainContext<Type> *parse();
	const string      &err() const;
};

#endif