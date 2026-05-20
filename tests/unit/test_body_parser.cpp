#include "framework.hpp"
#include "http/body.hpp"
#include "http/body_parser.hpp"
#include "http/boundary.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

namespace {

// Open a tmp file we can write a parser's output into and then read back.
struct TmpFile {
	FILE  *fp;
	string path;

	TmpFile() {
		char name[] = "/tmp/servio_test_XXXXXX";
		int  fd = mkstemp(name);
		path = name;
		fp = fdopen(fd, "w+");
	}
	~TmpFile() {
		if (fp) fclose(fp);
		unlink(path.c_str());
	}

	string contents() {
		fflush(fp);
		FILE *r = fopen(path.c_str(), "rb");
		if (!r) return "";
		char   buf[4096];
		string out;
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), r)) > 0)
			out.append(buf, n);
		fclose(r);
		return out;
	}
};

}  // namespace

// ---------------------------------------------------------- LengthedBodyParser

TEST(LengthedBodyParser, writesExactlyContentLength) {
	TmpFile             tmp;
	LengthedBodyParser  p(tmp.fp, 5);

	size_t consumed = p.consume("helloEXTRA", 10);
	ASSERT_EQ(consumed, (size_t)5);
	ASSERT_TRUE(p.isDone());
	ASSERT_FALSE(p.isError());
	ASSERT_STREQ(tmp.contents(), "hello");
}

TEST(LengthedBodyParser, handlesSplitFeeds) {
	TmpFile             tmp;
	LengthedBodyParser  p(tmp.fp, 11);

	ASSERT_EQ(p.consume("hello ", 6), (size_t)6);
	ASSERT_FALSE(p.isDone());
	ASSERT_EQ(p.consume("worldEXTRA", 10), (size_t)5);
	ASSERT_TRUE(p.isDone());
	ASSERT_STREQ(tmp.contents(), "hello world");
}

// ----------------------------------------------------------- ChunkedBodyParser

TEST(ChunkedBodyParser, decodesSingleChunkAndTerminator) {
	TmpFile            tmp;
	ChunkedBodyParser  p(tmp.fp);

	const string frame = "5\r\nhello\r\n0\r\n\r\n";
	size_t consumed = p.consume(frame.data(), frame.size());

	ASSERT_TRUE(p.isDone());
	ASSERT_FALSE(p.isError());
	ASSERT_EQ(consumed, frame.size());
	ASSERT_STREQ(tmp.contents(), "hello");
}

TEST(ChunkedBodyParser, handlesMultipleChunks) {
	TmpFile            tmp;
	ChunkedBodyParser  p(tmp.fp);

	const string frame = "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n";
	p.consume(frame.data(), frame.size());

	ASSERT_TRUE(p.isDone());
	ASSERT_STREQ(tmp.contents(), "hello world");
}

TEST(ChunkedBodyParser, errorsOnNonHexSizeLine) {
	TmpFile            tmp;
	ChunkedBodyParser  p(tmp.fp);
	const string frame = "X\r\nhello\r\n";
	p.consume(frame.data(), frame.size());
	ASSERT_TRUE(p.isError());
}

TEST(ChunkedBodyParser, errorsOnEmptySizeLine) {
	TmpFile            tmp;
	ChunkedBodyParser  p(tmp.fp);
	p.consume("\r\n", 2);
	ASSERT_TRUE(p.isError());
}

TEST(ChunkedBodyParser, resumesAcrossFeeds) {
	TmpFile            tmp;
	ChunkedBodyParser  p(tmp.fp);

	const string frame = "5\r\nhello\r\n0\r\n\r\n";
	for (size_t i = 0; i < frame.size(); ++i)
		p.consume(frame.data() + i, 1);

	ASSERT_TRUE(p.isDone());
	ASSERT_STREQ(tmp.contents(), "hello");
}

// --------------------------------------------------------- MultipartBodyParser

namespace {

// Builds a textbook multipart/form-data body for `parts` (each pair is name,
// content). Caller provides the boundary token.
string buildMultipart(const string &boundary, const string &name, const string &filename, const string &content) {
	string out;
	out += "--" + boundary + "\r\n";
	out += "Content-Disposition: form-data; name=\"" + name + "\"; filename=\"" + filename + "\"\r\n";
	out += "Content-Type: text/plain\r\n\r\n";
	out += content;
	out += "\r\n--" + boundary + "--\r\n";
	return out;
}

}  // namespace

TEST(MultipartBodyParser, writesSinglePartContent) {
	Boundary             b = Boundary::parse("multipart/form-data; boundary=BND").unwrap();
	map<int, BodyFile>   files;
	MultipartBodyParser  p(b, files);

	const string body = buildMultipart("BND", "file", "hello.txt", "hello world");
	p.consume(body.data(), body.size());

	ASSERT_TRUE(p.isDone());
	ASSERT_FALSE(p.isError());
	ASSERT_EQ(files.size(), (size_t)1);

	// The part file should contain exactly the upload bytes.
	BodyFile &part = files.begin()->second;
	fflush(part.file());

	FILE  *r = fopen(part.tmpPath().c_str(), "rb");
	ASSERT_TRUE(r != NULL);
	char   buf[256];
	size_t n = fread(buf, 1, sizeof(buf), r);
	fclose(r);
	ASSERT_STREQ(string(buf, n), "hello world");

	// Clean up the tmp file the parser created.
	unlink(part.tmpPath().c_str());
}

TEST(MultipartBodyParser, captureClientFilename) {
	Boundary             b = Boundary::parse("multipart/form-data; boundary=BND").unwrap();
	map<int, BodyFile>   files;
	MultipartBodyParser  p(b, files);

	const string body = buildMultipart("BND", "f", "upload-target.txt", "abc");
	p.consume(body.data(), body.size());

	ASSERT_TRUE(p.isDone());
	ASSERT_EQ(files.size(), (size_t)1);
	ASSERT_STREQ(files.begin()->second.clientFilename(), "upload-target.txt");
	unlink(files.begin()->second.tmpPath().c_str());
}

TEST(MultipartBodyParser, twoPartsLandInSeparateFiles) {
	Boundary             b = Boundary::parse("multipart/form-data; boundary=BND").unwrap();
	map<int, BodyFile>   files;
	MultipartBodyParser  p(b, files);

	string body;
	body += "--BND\r\n";
	body += "Content-Disposition: form-data; name=\"a\"; filename=\"a.txt\"\r\n\r\n";
	body += "AAA";
	body += "\r\n--BND\r\n";
	body += "Content-Disposition: form-data; name=\"b\"; filename=\"b.txt\"\r\n\r\n";
	body += "BBBB";
	body += "\r\n--BND--\r\n";

	p.consume(body.data(), body.size());

	ASSERT_TRUE(p.isDone());
	ASSERT_EQ(files.size(), (size_t)2);

	for (map<int, BodyFile>::iterator it = files.begin(); it != files.end(); ++it)
		unlink(it->second.tmpPath().c_str());
}

TEST(MultipartBodyParser, surviveFragmentedRecvs) {
	Boundary             b = Boundary::parse("multipart/form-data; boundary=BND").unwrap();
	map<int, BodyFile>   files;
	MultipartBodyParser  p(b, files);

	// Big content that will be needle-scanned across many feeds.
	string content;
	for (int i = 0; i < 1024; ++i)
		content += "ABCDEFGHIJKL";  // 12K bytes
	const string body = buildMultipart("BND", "f", "blob.bin", content);

	for (size_t i = 0; i < body.size(); ++i)
		p.consume(body.data() + i, 1);

	ASSERT_TRUE(p.isDone());
	ASSERT_EQ(files.size(), (size_t)1);

	BodyFile &part = files.begin()->second;
	fflush(part.file());

	struct stat st;
	ASSERT_EQ(stat(part.tmpPath().c_str(), &st), 0);
	ASSERT_EQ((size_t)st.st_size, content.size());

	unlink(part.tmpPath().c_str());
}
