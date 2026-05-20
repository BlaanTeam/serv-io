#include "lexer.hpp"

#include <cctype>
#include <cstring>

using namespace std;

const char *TokenNames[8] = {
    "WORD",
    "OPEN CURLY BRACE",
    "CLOSING CURLY BRACE",
    "SEMICOLON",
    "DOUBLE QOUTE",
    "SINGLE QOUTE",
    "END OF FILE",
    "UNKNOWN TOKEN"};

string name(int type) {
	if (type > (1 << 7))
		return "UNKNOWN";
	return TokenNames[(int)log2(int(type)) - 1];
}

// Token Class

Token::Token() {}

Token::Token(const TokenType &type, const string &value, const size_t &line) : _type(type), _value(value), _line(line) {}

// Token Getters

TokenType Token::type(void) const {
	return _type;
}

const string &Token::value(void) const {
	return _value;
}

size_t Token::line(void) const {
	return _line;
}

string Token::name() const {
	return ::name(_type);
}

// Lexer Class

Lexer::Lexer() {}

bool Lexer::tokenizer(ifstream &file) {
	char   chr;
	size_t line = 1;
	string value;

	file.get(chr);
	while (!file.eof()) {
		switch (chr) {
		case '#':
			while (chr != '\n' && !file.eof())
				file.get(chr);
			// fall through to the whitespace branch — the loop above stopped
			// on '\n', and we want to count the line + skip the byte uniformly.
			__attribute__((fallthrough));
		case ' ':
		case '\n':
			if (chr == '\n') line++;
			__attribute__((fallthrough));
		case '\r':
		case '\f':
		case '\v':
		case '\t':
			break;
		case '{':
		case '}':
			push_back(Token(chr == '}' ? CCURLY : OCURLY, string(1, chr), line));
			break;
		case '\"':
			// push_back(Token(DQOUTE, string(1, chr), line));
			file.get(chr);
			while (chr != '\"' && !file.eof()) {
				value += chr;
				file.get(chr);
			}
			push_back(Token(WORD, value, line));
			value.clear();
			if (chr != '\"')
				return false;
			break;
		case ';':
			push_back(Token(SEMICOLON, string(1, chr), line));
			break;
		default:
			if (!::isprint(chr)) {
				push_back(Token(_UNKNOWN, string(1, chr), line));
				push_back(Token(_EOF, "End Of File", line));
				return true;
			}
			string word;
			while (!strchr("{};\"", chr) && !isspace(chr) && !file.eof()) {
				if (chr == '\\')
					file.get(chr);
				word += string(1, chr);
				file.get(chr);
			}
			push_back(Token(WORD, word, line));
			continue;
		};
		file.get(chr);
	}
	push_back(Token(_EOF, "End Of File", line));

	// reset seek of the file to the beginning !
	file.clear();
	file.seekg(0);

	return true;
}
