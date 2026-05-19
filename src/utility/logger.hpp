#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <cerrno>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include "helpers.hpp"
#include "parser.hpp"

using namespace std;

class Logger {
	fstream _access;
	fstream _error;

	void logTime(fstream &file);

   public:
	Logger(const string &prefixFolder = PREFIX_FOLDER);
	void error(const char *file, int line, const string &msg);
	void notice(const string &msg);
	~Logger();
};

extern Logger logger;

#endif