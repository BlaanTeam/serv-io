#ifndef SERVIO_LEXER_HPP
#define SERVIO_LEXER_HPP

#include <math.h>

#include <deque>
#include <fstream>
#include <iostream>
#include <string>

extern const char *TokenNames[8];

enum TokenType {
	WORD = 1 << 1,
	OCURLY = 1 << 2,
	CCURLY = 1 << 3,
	SEMICOLON = 1 << 4,
	DQOUTE = 1 << 5,
	SQOUTE = 1 << 6,
	_EOF = 1 << 7,
	_UNKNOWN = 1 << 8,
};

std::string name(int type);

class Token {
	TokenType   _type;
	std::string _value;
	std::size_t _line;

   public:
	Token();
	Token(const TokenType &type, const std::string &value, const std::size_t &line);

	// Getters
	TokenType          type(void) const;
	const std::string &value(void) const;
	std::size_t        line(void) const;
	std::string        name() const;
};

class Lexer : public std::deque<Token> {
   public:
	Lexer();

	bool tokenizer(std::ifstream &file);
};

#endif
