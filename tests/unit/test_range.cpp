#include "framework.hpp"
#include "http/range.hpp"
#include "utility/result.hpp"

using servio::Result;

TEST(Range, parsesSingleClosedInterval) {
	Result<Range, std::string> r = Range::parse("bytes=100-199");
	ASSERT_TRUE(r.isOk());

	const std::vector<RangeSpecifier> &specs = r.unwrap().specifiers();
	ASSERT_EQ(specs.size(), (size_t)1);
	ASSERT_EQ(specs[0].rangeStart, (size_t)100);
	ASSERT_EQ(specs[0].rangeEnd,   (size_t)199);
	ASSERT_EQ(specs[0].type,       NON);
}

TEST(Range, parsesOpenSuffix) {
	Result<Range, std::string> r = Range::parse("bytes=500-");
	ASSERT_TRUE(r.isOk());
	const std::vector<RangeSpecifier> &specs = r.unwrap().specifiers();
	ASSERT_EQ(specs[0].rangeStart, (size_t)500);
	ASSERT_EQ(specs[0].type,       NOL);
}

TEST(Range, parsesSuffixLength) {
	Result<Range, std::string> r = Range::parse("bytes=-200");
	ASSERT_TRUE(r.isOk());
	const std::vector<RangeSpecifier> &specs = r.unwrap().specifiers();
	ASSERT_EQ(specs[0].rangeEnd, (size_t)200);
	ASSERT_EQ(specs[0].type,     NOF);
}

TEST(Range, rejectsMissingUnit) {
	ASSERT_TRUE(Range::parse("100-199").isErr());
}

TEST(Range, rejectsBackwardsInterval) {
	ASSERT_TRUE(Range::parse("bytes=200-100").isErr());
}

TEST(Range, errorMessageMentionsTheBadInput) {
	Result<Range, std::string> r = Range::parse("bytes=abc-xyz");
	ASSERT_TRUE(r.isErr());
	ASSERT_FALSE(r.unwrapErr().empty());
}

TEST(Range, getContentLengthClosedInterval) {
	Result<Range, std::string> r = Range::parse("bytes=10-19");
	ASSERT_TRUE(r.isOk());
	RangeSpecifier rs = r.unwrap().specifiers()[0];
	ASSERT_EQ(rs.contentLength((size_t)100), (size_t)10);
}

TEST(Range, getContentLengthOpenSuffix) {
	Result<Range, std::string> r = Range::parse("bytes=90-");
	ASSERT_TRUE(r.isOk());
	RangeSpecifier rs = r.unwrap().specifiers()[0];
	ASSERT_EQ(rs.contentLength((size_t)100), (size_t)10);
}

TEST(Range, defaultIsEmpty) {
	Range r;
	ASSERT_TRUE(r.empty());
}
