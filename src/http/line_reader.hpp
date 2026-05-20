#ifndef SERVIO_LINE_READER_HPP
#define SERVIO_LINE_READER_HPP

#include <deque>
#include <string>

#include "./streamsearch.hpp"
#include "utility/result.hpp"

using namespace std;

// Streaming line scanner. Wraps `StreamSearch` (needle = "\n") with an
// accumulator + a queue of completed lines.
//
// Used by `Request` for the start-line and headers and by
// `ChunkedBodyParser` for size lines and the trailer — all the
// "find a CRLF, hand me everything up to it" sites in the request path
// share this primitive.
//
// Bare-LF tolerance falls out of the needle choice (`\n`): if the line
// arrived as CRLF, `takeLine()` strips the trailing CR before yielding
// it. Mid-line CRs survive untouched (matches HTTP semantics).
class LineReader : private StreamSearch::Sink {
   public:
	LineReader();
	virtual ~LineReader();

	// Feed bytes from the recv buffer. Returns bytes consumed.
	// Multiple complete lines may queue up — drain them with takeLine().
	size_t feed(const char *buf, size_t len);

	// Pull the next complete line (CR/LF stripped). None when the
	// accumulator is mid-line.
	servio::Option<string> takeLine();

	// Bytes accumulated for the in-flight (incomplete) line. Useful for
	// enforcing header size caps.
	size_t pendingSize() const;

	void reset();

   private:
	// StreamSearch::Sink — receives non-needle bytes during scanning.
	virtual void onData(const char *data, size_t len);

	StreamSearch  _search;
	string        _pending;
	deque<string> _ready;
};

#endif
