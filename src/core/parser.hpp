#ifndef SERVIO_PARSER_HPP
#define SERVIO_PARSER_HPP

#include "ast.hpp"
#include "lexer.hpp"
#include "status_codes.hpp"

#ifndef PREFIX_FOLDER
#define PREFIX_FOLDER ""
#endif

class Parser {
	std::string _serr;
	Lexer       _lex;

	typedef std::pair<std::string, std::vector<std::string> > Directive;

   private:
	const Token &current();
	bool         accept(int type, const std::string &value = "");
	bool         expect(int type, const std::string &value = "");
	bool         updateDirectives(MainContext<std::vector<std::string> > *tree,
	                              MainContext<std::vector<std::string> > *parent = nullptr);

	Directive *parse_directive(Directive *_dir = nullptr);
	Directive *parse_http_dir(Directive *_dir = nullptr);
	Directive *parse_server_dir();
	Directive *parse_location_dir();

	MainContext<std::vector<std::string> > *parse_location();
	MainContext<std::vector<std::string> > *parse_server();
	MainContext<std::vector<std::string> > *parse_main();

	std::pair<bool, MainContext<Type> *> transfer(MainContext<std::vector<std::string> > *tree);

   public:
	Parser(std::ifstream &cfile);
	MainContext<Type> *parse();
	const std::string &err() const;
};

#endif
