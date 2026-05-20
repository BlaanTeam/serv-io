#include "framework.hpp"

#include <cstdlib>
#include <exception>

namespace test {

Suite& Suite::instance() {
	static Suite s;
	return s;
}

void Suite::add(const std::string& group, const std::string& name, TestFn fn) {
	Entry e;
	e.group = group;
	e.name = name;
	e.fn = fn;
	_tests.push_back(e);
}

namespace {
const char* kGreen = "\033[1;32m";
const char* kRed = "\033[1;31m";
const char* kDim = "\033[2m";
const char* kReset = "\033[0m";
}  // namespace

int Suite::run() {
	std::cout << kDim << "running " << _tests.size() << " tests" << kReset << "\n\n";

	int passed = 0;
	int failed = 0;
	std::vector<std::string> failures;

	for (size_t i = 0; i < _tests.size(); ++i) {
		const Entry& t = _tests[i];
		std::string  label = t.group + "." + t.name;
		try {
			t.fn();
			std::cout << "  " << kGreen << "ok  " << kReset << label << "\n";
			++passed;
		} catch (const Failure& f) {
			std::cout << "  " << kRed << "FAIL" << kReset << " " << label << "\n"
			          << "      " << f.msg() << "\n";
			failures.push_back(label + ":\n      " + f.msg());
			++failed;
		} catch (const std::exception& e) {
			std::string msg = std::string("uncaught exception: ") + e.what();
			std::cout << "  " << kRed << "FAIL" << kReset << " " << label << "\n"
			          << "      " << msg << "\n";
			failures.push_back(label + ":\n      " + msg);
			++failed;
		} catch (...) {
			std::cout << "  " << kRed << "FAIL" << kReset << " " << label << "\n"
			          << "      unknown exception\n";
			failures.push_back(label + ": unknown exception");
			++failed;
		}
	}

	std::cout << "\n";
	if (failed == 0) {
		std::cout << kGreen << passed << " passed" << kReset << ", 0 failed\n";
		return 0;
	}
	std::cout << passed << " passed, " << kRed << failed << " failed" << kReset << "\n";
	return 1;
}

}  // namespace test
