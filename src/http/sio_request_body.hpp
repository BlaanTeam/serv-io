#ifndef __BODY_H__
#define __BODY_H__

#include <stdio.h>

#include <map>
#include <string>

#include "./sio_boundary.hpp"
#include "./sio_header.hpp"
#include "./sio_streamsearch.hpp"
#include "utility/sio_helpers.hpp"
#include "utility/sio_utils.hpp"

using namespace std;

// Overall body lifecycle
#define BODY_INIT (1 << 0)
#define BODY_OPEN (1 << 1)
#define CHUNKED_BODY (1 << 2)
#define LENGTHED_BODY (1 << 3)
#define MULTIPARTED_BODY (1 << 4)
#define BODY_READ (CHUNKED_BODY | LENGTHED_BODY | MULTIPARTED_BODY)
#define BODY_DONE (1 << 5)
#define BODY_ERROR (1 << 6)

class Request;

class BodyFile {
	FILE  *_file;
	string _filename;

   public:
	Header _headers;
	BodyFile();
	~BodyFile();

	void addFile(FILE *file, const string &filename);
	void addHeader(const string &key, const string &value);
	void write(const char *data, size_t len);

	FILE  *getFile();
	string getFilename() const;

	string extractFilename();
};

// Streaming HTTP body parser. Three modes:
//  - LENGTHED: read exactly Content-Length bytes into a backing tmp file.
//  - CHUNKED : decode "<hex-size>\r\n<bytes>\r\n" frames, terminator size==0.
//  - MULTIPART: scan for boundary delimiters using a streamsearch-style
//    needle search; for each part, parse part headers then write the payload
//    to its own tmp file under /tmp/.servio_*_upload.io.
// Inherits Sink so the StreamSearch can deliver "info" (non-needle) bytes
// directly back here without a separate back-pointer object that would
// dangle on object copy.
class Body : public StreamSearch::Sink {
   public:
	Body();
	Body(const Body &copy);
	Body &operator=(const Body &rhs);
	~Body();

	void chooseState(Header &headers);
	void setState(int state);
	short getState() const;

	void openFile();
	void closeFile();

	// Feed raw bytes from the socket. Returns number of bytes consumed.
	size_t consume(const char *buf, size_t len);

	map<int, BodyFile> &getBodyFiles();
	int                 getFileno() const;

	void reset();

   private:
	// Lifecycle
	FILE  *_bodyFile;
	short  _bodyState;
	string _filename;

	// Lengthed
	size_t _contentLength;
	size_t _written;

	// Chunked sub-state
	enum ChunkPhase {
		CHUNK_SIZE_LINE,
		CHUNK_DATA,
		CHUNK_DATA_CR,
		CHUNK_DATA_LF,
		CHUNK_TRAILER_CR,
		CHUNK_TRAILER_LF
	};
	ChunkPhase _chunkPhase;
	string     _chunkSizeLine;
	size_t     _chunkSize;
	size_t     _chunkRemaining;

	// Multipart sub-state
	Boundary  _boundary;

	enum MultipartPhase {
		MP_PREAMBLE,         // scanning for first boundary; data discarded
		MP_AFTER_BOUNDARY,   // boundary found; inspect next 2 bytes
		MP_HEADERS,          // accumulating part headers until empty line
		MP_PART_BODY,        // scanning for next boundary; data → current file
		MP_EPILOGUE          // final boundary received; ignore remaining bytes
	};
	MultipartPhase _mpPhase;
	StreamSearch   _search;        // needle = "\r\n--<boundary>"
	string         _afterTail;     // 0-2 bytes pending in MP_AFTER_BOUNDARY
	string         _headerLine;    // current header line being assembled

	map<int, BodyFile> _bodyFiles;
	int                _fileIndex;
	bool               _partFileOpen;

   public:
	// StreamSearch::Sink: receives "info" (non-needle) bytes during scanning.
	virtual void onData(const char *data, size_t len);

   private:
	size_t consumeLengthed(const char *buf, size_t len);
	size_t consumeChunked(const char *buf, size_t len);
	size_t consumeMultipart(const char *buf, size_t len);

	void openPartFile();
	void closePartFile();
	void parsePartHeaderLine(const string &line);
};

#include "./sio_request.hpp"

#endif
