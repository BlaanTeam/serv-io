#ifndef SERVIO_LOGGER_HPP
#define SERVIO_LOGGER_HPP

#include <cerrno>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include "helpers.hpp"
#include "parser.hpp"

class Logger {
	std::fstream _access;
	std::fstream _error;

	void logTime(std::fstream &file);

   public:
	Logger(const std::string &prefixFolder = PREFIX_FOLDER);
	void error(const char *file, int line, const std::string &msg);
	void notice(const std::string &msg);
	~Logger();
};

extern Logger logger;

#endif
