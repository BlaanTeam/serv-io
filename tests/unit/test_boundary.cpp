#include "framework.hpp"
#include "http/boundary.hpp"
#include "utility/result.hpp"

using servio::Result;

TEST(Boundary, parsesSimpleHeader) {
	Result<Boundary, std::string> r = Boundary::parse("multipart/form-data; boundary=abc123");
	ASSERT_TRUE(r.isOk());
	ASSERT_STREQ(r.unwrap().value(), "abc123");
}

TEST(Boundary, isCaseInsensitiveOnMediaType) {
	Result<Boundary, std::string> r = Boundary::parse("Multipart/Form-Data; boundary=xyz");
	ASSERT_TRUE(r.isOk());
	ASSERT_STREQ(r.unwrap().value(), "xyz");
}

TEST(Boundary, stripsSurroundingQuotes) {
	Result<Boundary, std::string> r =
		Boundary::parse("multipart/form-data; boundary=\"--quoted--\"");
	ASSERT_TRUE(r.isOk());
	ASSERT_STREQ(r.unwrap().value(), "--quoted--");
}

TEST(Boundary, rejectsWrongMediaType) {
	Result<Boundary, std::string> r = Boundary::parse("text/plain; boundary=nope");
	ASSERT_TRUE(r.isErr());
}

TEST(Boundary, rejectsMissingBoundary) {
	Result<Boundary, std::string> r = Boundary::parse("multipart/form-data");
	ASSERT_TRUE(r.isErr());
}

TEST(Boundary, rejectsEmptyBoundary) {
	Result<Boundary, std::string> r = Boundary::parse("multipart/form-data; boundary=");
	ASSERT_TRUE(r.isErr());
}

TEST(Boundary, errorCarriesAHumanMessage) {
	Result<Boundary, std::string> r = Boundary::parse("text/plain");
	ASSERT_TRUE(r.isErr());
	ASSERT_FALSE(r.unwrapErr().empty());
}

TEST(Boundary, defaultIsEmpty) {
	Boundary b;
	ASSERT_TRUE(b.empty());
}
