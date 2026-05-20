#include "framework.hpp"
#include "http/line_reader.hpp"

#include <string>

using namespace std;
using servio::Option;

namespace {

// Drains all currently queued lines, then keeps re-feeding the leftover
// slice of input until no more progress is possible — mirroring how
// real callers (Request, ChunkedBodyParser) interleave feed + takeLine.
vector<string> drainAll(LineReader &r, const string &in) {
	vector<string> out;
	size_t         i = 0;
	while (true) {
		Option<string> line = r.takeLine();
		if (line.isSome()) {
			out.push_back(line.unwrap());
			continue;
		}
		if (i >= in.size())
			break;
		const size_t taken = r.feed(in.data() + i, in.size() - i);
		i += taken;
		if (taken == 0) break;
	}
	return out;
}

}  // namespace

TEST(LineReader, takeReturnsNoneWhenEmpty) {
	LineReader r;
	ASSERT_TRUE(r.takeLine().isNone());
}

TEST(LineReader, splitsOnCRLF) {
	LineReader r;
	const string in = "hello\r\nworld";

	vector<string> lines = drainAll(r, in);
	ASSERT_EQ(lines.size(), (size_t)1);
	ASSERT_STREQ(lines[0], "hello");
	// "world" is in-flight; not yet a complete line.
	ASSERT_EQ(r.pendingSize(), (size_t)5);
}

TEST(LineReader, splitsOnBareLF) {
	LineReader r;
	const string in = "hello\nworld\n";

	vector<string> lines = drainAll(r, in);
	ASSERT_EQ(lines.size(), (size_t)2);
	ASSERT_STREQ(lines[0], "hello");
	ASSERT_STREQ(lines[1], "world");
}

TEST(LineReader, stripsCROnlyIfTrailing) {
	LineReader  r;
	const string in = "a\rb\r\n";   // mid-line CR survives, trailing CR stripped
	r.feed(in.data(), in.size());

	Option<string> first = r.takeLine();
	ASSERT_TRUE(first.isSome());
	ASSERT_STREQ(first.unwrap(), "a\rb");
}

TEST(LineReader, multipleLinesAcrossFeeds) {
	LineReader r;
	const string in = "alpha\r\nbeta\r\ngamma\r\n";

	vector<string> lines = drainAll(r, in);
	ASSERT_EQ(lines.size(), (size_t)3);
	ASSERT_STREQ(lines[0], "alpha");
	ASSERT_STREQ(lines[1], "beta");
	ASSERT_STREQ(lines[2], "gamma");
}

TEST(LineReader, feedStopsAfterOneLine) {
	LineReader r;
	const string in = "one\ntwo\nthree\n";
	const size_t taken = r.feed(in.data(), in.size());
	ASSERT_EQ(taken, (size_t)4);  // "one\n"
	ASSERT_STREQ(r.takeLine().unwrap(), "one");
	ASSERT_TRUE(r.takeLine().isNone());
}

TEST(LineReader, emptyLineYieldsEmptyString) {
	LineReader  r;
	const string in = "\r\n";       // the canonical header-terminator
	r.feed(in.data(), in.size());

	Option<string> line = r.takeLine();
	ASSERT_TRUE(line.isSome());
	ASSERT_STREQ(line.unwrap(), "");
}

TEST(LineReader, byteByByteFeed) {
	LineReader  r;
	const string in = "line one\r\nline two\r\n";
	vector<string> lines;
	for (size_t i = 0; i < in.size(); ++i) {
		r.feed(in.data() + i, 1);
		Option<string> line;
		while ((line = r.takeLine()).isSome())
			lines.push_back(line.unwrap());
	}
	ASSERT_EQ(lines.size(), (size_t)2);
	ASSERT_STREQ(lines[0], "line one");
	ASSERT_STREQ(lines[1], "line two");
}

TEST(LineReader, pendingSizeTracksAccumulator) {
	LineReader r;
	r.feed("partial", 7);
	ASSERT_EQ(r.pendingSize(), (size_t)7);
	r.feed("-more", 5);
	ASSERT_EQ(r.pendingSize(), (size_t)12);
	r.feed("\n", 1);
	ASSERT_EQ(r.pendingSize(), (size_t)0);
}

TEST(LineReader, resetClearsPendingAndQueue) {
	LineReader r;
	r.feed("queued\nin-flight", 16);
	ASSERT_TRUE(r.takeLine().isSome());  // pop "queued"
	r.reset();
	ASSERT_TRUE(r.takeLine().isNone());
	ASSERT_EQ(r.pendingSize(), (size_t)0);
}

TEST(LineReader, feedReportsBytesConsumedThroughTerminator) {
	LineReader r;
	const string in = "hi\r\nremainder";
	const size_t consumed = r.feed(in.data(), in.size());
	// feed stops after the first complete line: "hi\r\n" = 4 bytes.
	ASSERT_EQ(consumed, (size_t)4);
	ASSERT_EQ(r.pendingSize(), (size_t)0);
	ASSERT_STREQ(r.takeLine().unwrap(), "hi");
}
