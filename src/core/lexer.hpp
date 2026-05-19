#ifndef __LEXER_H__
#define __LEXER_H__

#include <math.h>

#include <deque>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

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

string name(int type);

class Token {
	TokenType _type;
	string    _value;
	size_t    _line;

   public:
	Token();
	Token(const TokenType &type, const string &value, const size_t &line);

	// Getters
	TokenType     type(void) const;
	const string &value(void) const;
	size_t        line(void) const;
	string        name() const;
};

class Lexer : public deque<Token> {
   public:
	Lexer();

	bool tokenizer(ifstream &file);
};

#endif