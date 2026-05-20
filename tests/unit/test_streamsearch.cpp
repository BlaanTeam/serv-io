#include "framework.hpp"
#include "http/streamsearch.hpp"

#include <string>
#include <vector>

using namespace std;

namespace {

// Sink that records every onData call and concatenates everything into one
// flat buffer so tests can compare against the expected non-needle bytes.
class CapturingSink : public StreamSearch::Sink {
   public:
	string                 data;
	vector<size_t>         chunkSizes;
	int                    calls;

	CapturingSink() : calls(0) {}

	virtual void onData(const char *buf, size_t len) {
		data.append(buf, len);
		chunkSizes.push_back(len);
		++calls;
	}
};

}  // namespace

TEST(StreamSearch, findsNeedleInOneFeed) {
	CapturingSink sink;
	StreamSearch  ss;
	ss.init("--boundary", &sink);

	const string payload = "prefix data here--boundarymore data";
	size_t consumed = ss.feed(payload.data(), payload.size());

	ASSERT_TRUE(ss.matched());
	ASSERT_EQ(consumed, string("prefix data here--boundary").size());
	ASSERT_STREQ(sink.data, "prefix data here");
}

TEST(StreamSearch, matchAtStartEmitsNoData) {
	CapturingSink sink;
	StreamSearch  ss;
	ss.init("XYZ", &sink);

	size_t consumed = ss.feed("XYZrest", 7);
	ASSERT_TRUE(ss.matched());
	ASSERT_EQ(consumed, (size_t)3);
	ASSERT_STREQ(sink.data, "");
}

TEST(StreamSearch, noMatchYieldsEverythingAsData) {
	CapturingSink sink;
	StreamSearch  ss;
	ss.init("NEEDLE", &sink);

	const string payload = "no needle here at all";
	size_t consumed = ss.feed(payload.data(), payload.size());

	ASSERT_FALSE(ss.matched());
	ASSERT_EQ(consumed, payload.size());
	ASSERT_STREQ(sink.data, payload);
}

TEST(StreamSearch, splitsAcrossChunks) {
	CapturingSink sink;
	StreamSearch  ss;
	ss.init("\r\n--AAA", &sink);

	// Feed byte-by-byte to stress the cross-chunk lookbehind.
	const string payload = "hello\r\n--AAAtail";
	size_t       total = 0;
	for (size_t i = 0; i < payload.size() && !ss.matched(); ++i)
		total += ss.feed(payload.data() + i, 1);

	ASSERT_TRUE(ss.matched());
	// Bytes consumed up to and including the needle ("hello\r\n--AAA" = 12).
	ASSERT_EQ(total, (size_t)12);
	ASSERT_STREQ(sink.data, "hello");
}

TEST(StreamSearch, falsePartialMatchRewinds) {
	// Needle starts with two repeating bytes; a near-match should rewind and
	// still find the real needle later in the stream.
	CapturingSink sink;
	StreamSearch  ss;
	ss.init("AAB", &sink);

	const string payload = "AAAAB-tail";
	size_t consumed = ss.feed(payload.data(), payload.size());

	ASSERT_TRUE(ss.matched());
	ASSERT_EQ(consumed, (size_t)5);            // "AAAAB"
	ASSERT_STREQ(sink.data, "AA");             // two A's before the real match
}

TEST(StreamSearch, resumeAfterClearMatchContinuesScanning) {
	CapturingSink sink;
	StreamSearch  ss;
	ss.init("|END|", &sink);

	const string payload = "first|END|second|END|tail";
	size_t       i = 0;
	size_t       consumed = ss.feed(payload.data() + i, payload.size() - i);
	ASSERT_TRUE(ss.matched());
	ASSERT_EQ(consumed, (size_t)10);           // "first|END|"
	ASSERT_STREQ(sink.data, "first");
	ss.clearMatch();
	i += consumed;

	consumed = ss.feed(payload.data() + i, payload.size() - i);
	ASSERT_TRUE(ss.matched());
	// Cumulative data so far: "first" + "second"
	ASSERT_STREQ(sink.data, "firstsecond");
}

TEST(StreamSearch, emptyFeedReturnsZero) {
	CapturingSink sink;
	StreamSearch  ss;
	ss.init("x", &sink);
	ASSERT_EQ(ss.feed("", 0), (size_t)0);
	ASSERT_FALSE(ss.matched());
}

TEST(StreamSearch, resetClearsState) {
	CapturingSink sink;
	StreamSearch  ss;
	ss.init("ab", &sink);
	ss.feed("a", 1);     // partial match in flight
	ss.reset();
	ASSERT_FALSE(ss.matched());

	size_t consumed = ss.feed("b", 1);
	// After reset, the lone 'b' should be flushed as non-needle data, not
	// treated as the second half of a previously-pending partial match.
	ASSERT_FALSE(ss.matched());
	ASSERT_EQ(consumed, (size_t)1);
	ASSERT_STREQ(sink.data, "b");
}
