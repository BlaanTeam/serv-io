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

// ------------------------------------------------------------- match() -----

TEST(Option, matchInvokesSomeBranch) {
	int  captured = -1;
	bool wentNone = false;
	Some(42).match(
	    [&](const int &v) { captured = v; },
	    [&] { wentNone = true; });
	ASSERT_EQ(captured, 42);
	ASSERT_FALSE(wentNone);
}

TEST(Option, matchInvokesNoneBranch) {
	int  captured = -1;
	bool wentNone = false;
	None<int>().match(
	    [&](const int &v) { captured = v; },
	    [&] { wentNone = true; });
	ASSERT_TRUE(wentNone);
	ASSERT_EQ(captured, -1);
}

TEST(Option, matchToReturnsValue) {
	int len = Some(string("hello")).matchTo<int>(
	    [](const string &s) { return (int)s.size(); },
	    [] { return 0; });
	ASSERT_EQ(len, 5);

	int zero = None<string>().matchTo<int>(
	    [](const string &s) { return (int)s.size(); },
	    [] { return 0; });
	ASSERT_EQ(zero, 0);
}

TEST(Result, matchDispatchesOnDiscriminant) {
	int    value = -1;
	string error;
	auto   onOk  = [&](const int &v) { value = v; };
	auto   onErr = [&](const string &e) { error = e; };

	Result<int, string>::ok(7).match(onOk, onErr);
	ASSERT_EQ(value, 7);
	ASSERT_TRUE(error.empty());

	value = -1;
	Result<int, string>::err("boom").match(onOk, onErr);
	ASSERT_STREQ(error, "boom");
	ASSERT_EQ(value, -1);
}

// ------------------------------------------------------------------ Unit ---

TEST(Unit, equalsItself) {
	servio::Unit a, b;
	ASSERT_TRUE(a == b);
	ASSERT_FALSE(a != b);
}

TEST(Unit, fitsInsideResult) {
	Result<servio::Unit, string> ok = Result<servio::Unit, string>::ok(servio::Unit());
	ASSERT_TRUE(ok.isOk());

	Result<servio::Unit, string> bad = Result<servio::Unit, string>::err("nope");
	ASSERT_TRUE(bad.isErr());
	ASSERT_STREQ(bad.unwrapErr(), "nope");
}
