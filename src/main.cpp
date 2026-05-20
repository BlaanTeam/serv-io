#include <cstdlib>
#include <iostream>

#include "server.hpp"
#include "utility/result.hpp"

int main(int ac, char *const *av) {
	handleSignals();
	return servio_run(ac, av).matchTo<int>(
	    [](const servio::Unit &) { return EXIT_SUCCESS; },
	    [](const std::string &msg) {
		    std::cerr << "servio: " << msg << std::endl;
		    return EXIT_FAILURE;
	    });
}
