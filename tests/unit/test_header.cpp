#include "framework.hpp"
#include "http/header.hpp"

TEST(Header, addAndGetIsCaseInsensitive) {
	Header h;
	h.add("Content-Type", "text/plain");
	ASSERT_STREQ(h.get("Content-Type"), "text/plain");
	ASSERT_STREQ(h.get("content-type"), "text/plain");
	ASSERT_STREQ(h.get("CONTENT-TYPE"), "text/plain");
}

TEST(Header, missingKeyReturnsEmpty) {
	Header h;
	ASSERT_STREQ(h.get("X-Not-There"), "");
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
	ASSERT_STREQ(h.get("X-Custom"), "padded value");
}

TEST(Header, tryGetReturnsSomeForPresentKey) {
	Header h;
	h.add("Content-Length", "42");
	servio::Option<string> v = h.tryGet("content-length");
	ASSERT_TRUE(v.isSome());
	ASSERT_STREQ(v.unwrap(), "42");
}

TEST(Header, tryGetReturnsNoneForMissingKey) {
	Header h;
	ASSERT_TRUE(h.tryGet("Authorization").isNone());
}
