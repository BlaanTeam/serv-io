#include "logger.hpp"

Logger::Logger(const string &prefixFolder) {
	_access.open(joinPath(prefixFolder, "logs/access.log"), ios::in | ios::out | ios::app);
	if (_access.fail()) {
		cerr << "[warning] open: \"" << joinPath(prefixFolder, "logs/access.log") << "\": " << strerror(errno) << endl;
	}
	_error.open(joinPath(prefixFolder, "logs/error.log"), ios::in | ios::out | ios::app);
	if (_error.fail()) {
		cerr << "[warning] open: \"" << joinPath(prefixFolder, "logs/error.log") << "\": " << strerror(errno) << endl;
	}
}

void Logger::logTime(fstream &file) {
	time_t now = time(0);
	char   buff[80];
	strftime(buff, sizeof buff, "%Y-%m-%d %X ", localtime(&now));
	file << buff;
}

void Logger::error(const char *file, int line, const string &msg) {
	logTime(_error);
	_error << file << ':' << line << " -- " << msg << endl;
}

void Logger::notice(const string &msg) {
	logTime(_access);
	_access << "-- " << msg << endl;
}

Logger::~Logger() {
	_access.close();
	_error.close();
}

Logger logger;