#include <cstdlib>
#include <iostream>

#include "server.hpp"
#include "utility/result.hpp"

using namespace std;

namespace {

// `match` functors for the top-level `Result<Unit, string>` from servio_run.
// Putting them in named structs (instead of trying to invent a C++98 lambda)
// keeps the fold-style entry point readable.
struct OnClean {
	int operator()(const servio::Unit &) const { return EXIT_SUCCESS; }
};

struct OnFatal {
	int operator()(const string &msg) const {
		cerr << "servio: " << msg << endl;
		return EXIT_FAILURE;
	}
};

}  // namespace

int main(int ac, char *const *av) {
	handleSignals();
	return servio_run(ac, av).matchTo<int>(OnClean(), OnFatal());
}
