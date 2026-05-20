#ifndef SERVIO_BODY_PARSER_HPP
#define SERVIO_BODY_PARSER_HPP

#include <stdio.h>

#include <map>
#include <string>

#include "./body.hpp"
#include "./boundary.hpp"
#include "./line_reader.hpp"
#include "./streamsearch.hpp"
#include "utility/state_machine.hpp"

// Strategy interface for streaming HTTP body parsers.
//
// `consume` is fed raw bytes as they arrive on the socket and returns the
// number of bytes it actually used. It must be safe to call repeatedly across
// recv() boundaries — partial frames are buffered internally.
//
// Concrete strategies live below; `Body::chooseStrategy` picks one based on
// the request headers (factory).
class BodyParser {
   public:
	virtual ~BodyParser();

	virtual std::size_t consume(const char *buf, std::size_t len) = 0;
	virtual bool        isDone()  const = 0;
	virtual bool        isError() const = 0;
};

// Reads exactly Content-Length bytes into the staging file.
class LengthedBodyParser : public BodyParser {
   public:
	LengthedBodyParser(FILE *dst, std::size_t contentLength);

	virtual std::size_t consume(const char *buf, std::size_t len);
	virtual bool        isDone()  const;
	virtual bool        isError() const;

   private:
	FILE        *_dst;
	std::size_t  _contentLength;
	std::size_t  _written;
};

// Decodes the "<hex-size>\r\n<bytes>\r\n…0\r\n\r\n" Transfer-Encoding=chunked
// framing. Size lines and trailer headers are read through a `LineReader`;
// the data span itself is byte-counted.
class ChunkedBodyParser : public BodyParser {
   public:
	enum ChunkPhase {
		ReadSizeLine,    // pull "<hex>" via LineReader
		ReadData,        // count exactly _chunkRemaining bytes into _dst
		ReadDataCRLF,    // skip the trailing CRLF after a data chunk
		ReadTrailerLine  // accept (and discard) trailer header lines until empty
	};

	explicit ChunkedBodyParser(FILE *dst);

	virtual std::size_t consume(const char *buf, std::size_t len);
	virtual bool        isDone()  const;
	virtual bool        isError() const;

   private:
	FILE                      *_dst;
	servio::Phase<ChunkPhase>  _phase;
	LineReader                 _lineReader;
	std::size_t                _chunkRemaining;
	bool                       _done;
	bool                       _error;
};

// Parses multipart/form-data using a streamsearch-style needle scanner on
// "\r\n--<boundary>". Each part's bytes land in its own BodyFile, populated
// in the caller-supplied map.
class MultipartBodyParser : public BodyParser, public StreamSearch::Sink {
   public:
	enum MultipartPhase {
		Preamble,        // scanning for the very first boundary; data discarded
		AfterBoundary,   // boundary just hit; inspect the next 2 bytes
		ReadHeaders,     // accumulating part headers until the empty line
		ReadPartBody,    // scanning for the next boundary; data -> current part file
		Epilogue         // final boundary received; ignore remaining bytes
	};

	MultipartBodyParser(const Boundary &boundary, std::map<int, BodyFile> &files);

	virtual std::size_t consume(const char *buf, std::size_t len);
	virtual bool        isDone()  const;
	virtual bool        isError() const;

	// StreamSearch::Sink — receives non-needle bytes during scanning.
	virtual void onData(const char *data, std::size_t len);

   private:
	void openPartFile();
	void closePartFile();
	void parseHeaderLine(const std::string &line);

	StreamSearch                  _search;        // needle = "\r\n--<boundary>"
	std::map<int, BodyFile>      &_files;
	servio::Phase<MultipartPhase> _phase;
	std::string                   _afterTail;     // 0-2 bytes pending in AfterBoundary
	std::string                   _headerLine;    // current header line being assembled
	int                           _fileIndex;
	bool                          _partOpen;
	bool                          _done;
	bool                          _error;
};

#endif
