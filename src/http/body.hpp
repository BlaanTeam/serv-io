#ifndef SERVIO_BODY_HPP
#define SERVIO_BODY_HPP

#include <stdio.h>

#include <map>
#include <string>

#include "./header.hpp"

using namespace std;

class BodyParser;

// One per-part file produced by the multipart parser. Owns the FILE* and a
// few headers extracted from the part's preamble (Content-Disposition etc).
class BodyFile {
   public:
	Header _headers;

	BodyFile();
	~BodyFile();

	void   adoptFile(FILE *file, const string &filename);
	void   addHeader(const string &key, const string &value);
	void   write(const char *data, size_t len);

	FILE  *file();
	string tmpPath() const;          // server-side tmp path (e.g., /tmp/.servio_*.io)
	string clientFilename();         // filename advertised by the client (Content-Disposition)

   private:
	FILE  *_file;
	string _filename;
};

// Streaming HTTP body coordinator. Picks one of three BodyParser strategies
// based on the request headers (factory), then delegates the actual decoding
// to it. Owns the backing tmp file used as CGI stdin and the map of per-part
// files used by multipart uploads.
class Body {
   public:
	Body();
	Body(const Body &copy);
	Body &operator=(const Body &rhs);
	~Body();

	// Inspect the request headers, pick a BodyParser, and open the staging
	// tmp file. Safe to call once per request.
	void chooseStrategy(Header &headers);

	// Feed bytes from the socket. Returns bytes consumed.
	size_t consume(const char *buf, size_t len);

	bool isDone()  const;
	bool isError() const;

	int                 fileno() const;
	map<int, BodyFile> &bodyFiles();

	void closeFile();
	void reset();

   private:
	void openTmpFile();
	void destroyParser();

	BodyParser        *_parser;
	FILE              *_bodyFile;
	string             _bodyFilePath;
	map<int, BodyFile> _bodyFiles;
	bool               _strategyChosen;
	bool               _noBody;       // headers picked no parser at all
};

#endif
