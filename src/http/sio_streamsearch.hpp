#ifndef __STREAM_SEARCH_H__
#define __STREAM_SEARCH_H__

#include <cstddef>
#include <string>
#include <vector>

using namespace std;

// Streaming needle search inspired by https://www.npmjs.com/package/streamsearch
// (Boyer-Moore-Horspool variant that handles needles spanning chunk boundaries).
//
// Usage:
//   StreamSearch::Sink* sink = ...;        // your callback target
//   StreamSearch ss;
//   ss.init("\r\n--boundary", sink);
//   while (have data) {
//       size_t consumed = ss.feed(buf, len);
//       if (ss.matched()) {
//           // needle was found; the bytes preceding it (minus the needle) were
//           // delivered via sink->onData. `consumed` bytes of `buf` were used.
//           // The caller may now switch state and feed buf+consumed for the next phase.
//           ss.clearMatch();
//       }
//   }
class StreamSearch {
   public:
	class Sink {
	   public:
		virtual ~Sink();
		virtual void onData(const char* data, size_t len) = 0;
	};

	StreamSearch();
	~StreamSearch();

	void init(const string& needle, Sink* sink);

	// Push bytes through the search. Returns number of bytes from `buf` that were
	// consumed. On a match, feed stops at the byte right after the needle, with
	// `matched()` returning true.
	size_t feed(const char* buf, size_t len);

	bool matched() const;
	void clearMatch();
	void reset();

	const string& needle() const;

   private:
	void scan(const char* buf, size_t len, size_t& consumed);

	string         _needle;
	size_t         _matchPos;   // length of partial-match prefix carried across calls
	bool           _matched;
	Sink*          _sink;
};

#endif
