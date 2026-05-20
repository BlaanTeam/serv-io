#include "options.hpp"

#include "utility/result.hpp"

using namespace std;
using servio::Result;
using servio::Unit;

namespace {

// Functors for the `Config::load(...).match(onOk, onErr)` fold below.
struct ConfigLoaded {
	void operator()(const Unit &) const {}
};

struct ConfigLoadFailed {
	const string *path;
	bool         *failed;
	void          operator()(const string &msg) const {
		cerr << NAME ": \"" << *path << "\" failed: " << msg << endl;
		*failed = true;
	}
};

}  // namespace

static void _display_version(void) {
	cerr << "servio version: " << (NAME "/" VERSION) << endl;
}

static void _display_help(void) {
	cerr << NAME << " version: " << (NAME "/" VERSION) << endl;
	cerr << "Usage: " NAME " [-hvtT] [-c filename]" << endl;

	cerr.setf(ios::left);
	cerr << "\nOptions:" << endl;
	cerr << setw(14) << "  -h"            << ": this help" << endl;
	cerr << setw(14) << "  -v"            << ": show version and exit" << endl;
	cerr << setw(14) << "  -t"            << ": test configuration and exit" << endl;
	cerr << setw(14) << "  -T"            << ": test configuration, dump it and exit" << endl;
	cerr << setw(14) << "  -c filename"   << ": configuration file path (example: examples/servio.conf)" << endl;
}

// Try to open + parse the config; print success / failure to stderr in a
// way that matches the historical nginx-style `servio -t` output.
static bool _testConfiguration(Config &config) {
	Result<Unit, string> r = config.parse();
	const bool success = r.isOk();
	cerr << NAME ": configuration file \"" << config.path()
	     << "\" test is " << (success ? "successful" : "failed");
	if (!success)
		cerr << " (" << r.unwrapErr() << ")";
	cerr << endl;
	return success;
}

static void _dumpConfiguration(Config &config) {
	if (_testConfiguration(config))
		config.displayContent();
}

bool parse_options(const int &ac, char *const *av, Config &config) {
	int    opt, flags = 0;
	string opt_name, path;

	opterr = 0;  // suppress getopt's own error messages
	while ((opt = getopt(ac, av, "hvtTc:")) != -1) {
		switch (opt) {
		case 'h': flags |= HELP_OPT;      break;
		case 'v': flags |= VERSION_OPT;   break;
		case 't': flags |= CONF_TEST_OPT; break;
		case 'T': flags |= CONF_DUMP_OPT; break;
		case 'c':
			flags |= CONF_PATH_OPT;
			path = optarg;
			break;
		case '?':
			flags = UNKNOWN_OPT;
			opt_name = string(1, optopt);
			goto unknown;
		}
	}
unknown:

	if ((flags & UNKNOWN_OPT) || optind < ac) {
		if (optind < ac && (flags & ~UNKNOWN_OPT))
			opt_name = av[optind];
		if (optopt == 'c')
			cerr << NAME ": option \"-c\" requires file name" << endl;
		else
			cerr << NAME ": invalid option: \"" << opt_name << "\"" << endl;
		return false;
	}

	if (flags & (VERSION_OPT | HELP_OPT | CONF_PATH_OPT | CONF_TEST_OPT | CONF_DUMP_OPT)) {
		if (flags & VERSION_OPT) _display_version();
		if (flags & HELP_OPT)    _display_help();

		if (flags & CONF_PATH_OPT) {
			bool             loadFailed = false;
			ConfigLoaded     onOk;
			ConfigLoadFailed onErr = { &path, &loadFailed };
			config.load(path).match(onOk, onErr);
			if (loadFailed) return false;
		}
		if (flags & CONF_TEST_OPT) _testConfiguration(config);
		if (flags & CONF_DUMP_OPT) _dumpConfiguration(config);

		return (flags == CONF_PATH_OPT);
	}
	return true;
}
