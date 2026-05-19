#include "server.hpp"

int main(int ac, char *const *av) {
	try {
		handleSignals();
		servio_init(ac, av);
	} catch (...) {
		cerr << "servio: " << strerror(errno) << endl;
	}
}