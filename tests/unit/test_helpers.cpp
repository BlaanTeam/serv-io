#include "framework.hpp"
#include "utility/helpers.hpp"

#include <string>

using namespace std;

TEST(Helpers, normpathRejectsRelative) {
	ASSERT_TRUE(normpath("no-leading-slash").isNone());
}

TEST(Helpers, normpathCollapsesDotSegments) {
	ASSERT_STREQ(normpath("/a/./b/../c/").unwrap(), "/a/c/");
}

TEST(Helpers, normpathDetectsTraversalEscape) {
	ASSERT_TRUE(normpath("/../etc/passwd").isNone());
}

TEST(Helpers, normpathRoot) {
	ASSERT_STREQ(normpath("/").unwrap(), "/");
}

TEST(Helpers, normpathPreservesTrailingSlash) {
	ASSERT_STREQ(normpath("/dir/").unwrap(), "/dir/");
	ASSERT_STREQ(normpath("/dir").unwrap(),  "/dir");
}

TEST(Helpers, trimSymmetric) {
	string s = "   hello world   ";
	trim(s);
	ASSERT_STREQ(s, "hello world");
}

TEST(Helpers, trimWithCustomSeparators) {
	string s = "\"quoted\"";
	trim(s, "\"");
	ASSERT_STREQ(s, "quoted");
}

TEST(Helpers, ltrimAndRtrimIndependent) {
	string a = "  x  ";
	ltrim(a);
	ASSERT_STREQ(a, "x  ");

	string b = "  x  ";
	rtrim(b);
	ASSERT_STREQ(b, "  x");
}

TEST(Helpers, iequalStringIsCaseInsensitive) {
	ASSERT_TRUE(iequalString("Content-Type", "content-type"));
	ASSERT_TRUE(iequalString("CHUNKED", "Chunked"));
	ASSERT_FALSE(iequalString("Content-Type", "Content-Length"));
	ASSERT_FALSE(iequalString("abc", "abcd"));
}

TEST(Helpers, joinPathHandlesSlashes) {
	ASSERT_STREQ(joinPath("/a", "b"),   "/a/b");
	ASSERT_STREQ(joinPath("/a/", "b"),  "/a/b");
	ASSERT_STREQ(joinPath("/a", "/b"),  "/a/b");
	ASSERT_STREQ(joinPath("/a/", "/b"), "/a/b");
	ASSERT_STREQ(joinPath("",   "b"),   "b");
	ASSERT_STREQ(joinPath("/a", ""),    "/a");
}

TEST(Helpers, StringICaseCompareOrderIsCaseFolded) {
	StringICaseCompare cmp;
	ASSERT_TRUE(cmp("apple",  "Banana"));   // 'a' < 'b' regardless of case
	ASSERT_FALSE(cmp("BANANA", "apple"));
	ASSERT_FALSE(cmp("Apple",  "apple"));   // equal under case folding
}
