#include "framework.hpp"
#include "http/header.hpp"

using namespace std;

TEST(Header, addAndGetIsCaseInsensitive) {
	Header h;
	h.add("Content-Type", "text/plain");
	ASSERT_STREQ(h.get("Content-Type").unwrap(), "text/plain");
	ASSERT_STREQ(h.get("content-type").unwrap(), "text/plain");
	ASSERT_STREQ(h.get("CONTENT-TYPE").unwrap(), "text/plain");
}

TEST(Header, missingKeyReturnsNone) {
	Header h;
	ASSERT_TRUE(h.get("X-Not-There").isNone());
}

TEST(Header, foundReturnsTrueForPresentKey) {
	Header h;
	h.add("Host", "example.com");
	ASSERT_TRUE(h.found("Host"));
	ASSERT_TRUE(h.found("host"));
	ASSERT_FALSE(h.found("X-Other"));
}

TEST(Header, getTrimsSurroundingSpaces) {
	Header h;
	h.add("X-Custom", "   padded value   ");
	ASSERT_STREQ(h.get("X-Custom").unwrap(), "padded value");
}

TEST(Header, getReturnsSomeForPresentKey) {
	Header h;
	h.add("Content-Length", "42");
	servio::Option<string> v = h.get("content-length");
	ASSERT_TRUE(v.isSome());
	ASSERT_STREQ(v.unwrap(), "42");
}
