#include "framework.hpp"
#include "utility/result.hpp"

#include <string>

using namespace std;
using servio::Option;
using servio::Result;
using servio::Some;
using servio::None;

// ---------------------------------------------------------------- Option<T> --

TEST(Option, defaultIsNone) {
	Option<int> o;
	ASSERT_TRUE(o.isNone());
	ASSERT_FALSE(o.isSome());
}

TEST(Option, someCarriesValue) {
	Option<int> o = Some(42);
	ASSERT_TRUE(o.isSome());
	ASSERT_EQ(o.unwrap(), 42);
}

TEST(Option, unwrapOrFallsBackOnNone) {
	Option<int> n = None<int>();
	ASSERT_EQ(n.unwrapOr(7), 7);

	Option<int> s = Some(3);
	ASSERT_EQ(s.unwrapOr(7), 3);
}

namespace {
struct DoubleIt {
	int operator()(const int &x) const { return x * 2; }
};
struct Halve {
	int operator()(const int &x) const { return x / 2; }
};
struct EvenOnly {
	Option<int> operator()(const int &x) const {
		return (x % 2 == 0) ? Some(x) : None<int>();
	}
};
}  // namespace

TEST(Option, mapTransformsValue) {
	Option<int> r = Some(5).map<int>(DoubleIt());
	ASSERT_TRUE(r.isSome());
	ASSERT_EQ(r.unwrap(), 10);

	Option<int> n = None<int>().map<int>(DoubleIt());
	ASSERT_TRUE(n.isNone());
}

TEST(Option, andThenChainsFallibleOps) {
	Option<int> a = Some(4).andThen<int>(EvenOnly());
	ASSERT_TRUE(a.isSome());
	ASSERT_EQ(a.unwrap(), 4);

	Option<int> b = Some(5).andThen<int>(EvenOnly());
	ASSERT_TRUE(b.isNone());
}

// ----------------------------------------------------------- Result<T, E> ---

TEST(Result, okCarriesValue) {
	Result<int, string> r = Result<int, string>::ok(7);
	ASSERT_TRUE(r.isOk());
	ASSERT_FALSE(r.isErr());
	ASSERT_EQ(r.unwrap(), 7);
}

TEST(Result, errCarriesMessage) {
	Result<int, string> r = Result<int, string>::err("nope");
	ASSERT_TRUE(r.isErr());
	ASSERT_STREQ(r.unwrapErr(), "nope");
}

TEST(Result, unwrapOrFallsBackOnErr) {
	Result<int, string> r = Result<int, string>::err("x");
	ASSERT_EQ(r.unwrapOr(99), 99);

	Result<int, string> s = Result<int, string>::ok(3);
	ASSERT_EQ(s.unwrapOr(99), 3);
}

TEST(Result, mapTransformsOkOnly) {
	Result<int, string> r = Result<int, string>::ok(5).map<int>(DoubleIt());
	ASSERT_TRUE(r.isOk());
	ASSERT_EQ(r.unwrap(), 10);

	Result<int, string> e = Result<int, string>::err("oops").map<int>(DoubleIt());
	ASSERT_TRUE(e.isErr());
	ASSERT_STREQ(e.unwrapErr(), "oops");
}

TEST(Result, okAndErrProjectToOption) {
	Result<int, string> r = Result<int, string>::ok(42);
	ASSERT_TRUE(r.ok().isSome());
	ASSERT_TRUE(r.err().isNone());
	ASSERT_EQ(r.ok().unwrap(), 42);

	Result<int, string> e = Result<int, string>::err("bad");
	ASSERT_TRUE(e.ok().isNone());
	ASSERT_TRUE(e.err().isSome());
	ASSERT_STREQ(e.err().unwrap(), "bad");
}
