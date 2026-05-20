#ifndef SERVIO_TEST_FRAMEWORK_HPP
#define SERVIO_TEST_FRAMEWORK_HPP

// Minimal C++98 unit-test framework. No dependencies beyond the standard
// library. Inspired by GTest's TEST() macro layout, kept deliberately small
// so the whole framework lives in two short files.
//
// Usage:
//
//     #include "framework.hpp"
//
//     TEST(MyComponent, doesSomething) {
//         ASSERT_EQ(2 + 2, 4);
//         ASSERT_TRUE(somePredicate());
//     }
//
//     // tests/unit/main.cpp links everything and runs ::test::Suite::run().

#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace test {

class Failure {
   public:
	Failure(const std::string& msg) : _msg(msg) {}
	const std::string& msg() const { return _msg; }

   private:
	std::string _msg;
};

typedef void (*TestFn)();

class Suite {
   public:
	static Suite& instance();
	void          add(const std::string& group, const std::string& name, TestFn fn);
	int           run();

   private:
	struct Entry {
		std::string group;
		std::string name;
		TestFn      fn;
	};
	std::vector<Entry> _tests;
};

struct AutoRegister {
	AutoRegister(const std::string& group, const std::string& name, TestFn fn) {
		Suite::instance().add(group, name, fn);
	}
};

}  // namespace test

// Test declaration macro. Defines a function and a static registrator that
// adds it to the global test list at static-init time.
#define TEST(group, name)                                                                  \
	static void test_##group##_##name();                                                   \
	static ::test::AutoRegister reg_##group##_##name(#group, #name, &test_##group##_##name); \
	static void test_##group##_##name()

// Assertion macros. On failure they throw test::Failure with a location
// string so the runner can format the result without aborting the process.
#define TEST_FAIL_(msg)                                                            \
	do {                                                                           \
		std::ostringstream oss__;                                                  \
		oss__ << __FILE__ << ":" << __LINE__ << ": " << msg;                       \
		throw ::test::Failure(oss__.str());                                        \
	} while (0)

#define ASSERT_TRUE(expr)                                                          \
	do {                                                                           \
		if (!(expr)) TEST_FAIL_("ASSERT_TRUE(" #expr ") was false");               \
	} while (0)

#define ASSERT_FALSE(expr)                                                         \
	do {                                                                           \
		if (expr) TEST_FAIL_("ASSERT_FALSE(" #expr ") was true");                  \
	} while (0)

#define ASSERT_EQ(lhs, rhs)                                                        \
	do {                                                                           \
		if (!((lhs) == (rhs))) {                                                   \
			std::ostringstream m__;                                                \
			m__ << "ASSERT_EQ(" #lhs ", " #rhs ") failed: lhs=" << (lhs)            \
			    << " rhs=" << (rhs);                                               \
			TEST_FAIL_(m__.str());                                                 \
		}                                                                          \
	} while (0)

#define ASSERT_NE(lhs, rhs)                                                        \
	do {                                                                           \
		if ((lhs) == (rhs)) {                                                      \
			std::ostringstream m__;                                                \
			m__ << "ASSERT_NE(" #lhs ", " #rhs ") failed: both=" << (lhs);          \
			TEST_FAIL_(m__.str());                                                 \
		}                                                                          \
	} while (0)

#define ASSERT_STREQ(lhs, rhs)                                                     \
	do {                                                                           \
		std::string l__ = (lhs);                                                   \
		std::string r__ = (rhs);                                                   \
		if (l__ != r__) {                                                          \
			std::ostringstream m__;                                                \
			m__ << "ASSERT_STREQ failed:\n  lhs=\"" << l__ << "\"\n  rhs=\"" << r__ << "\"";   \
			TEST_FAIL_(m__.str());                                                 \
		}                                                                          \
	} while (0)

#endif
