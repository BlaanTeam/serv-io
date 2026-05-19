#include "options.hpp"

static void _display_version(void) {
	cerr << "servio version: " << (NAME "/" VERSION) << endl;
}

static void _display_help(void) {
	cerr << NAME << " version: " << (NAME "/" VERSION) << endl;
	cerr << "Usage: " NAME " [-hvtT] [-c filename]" << endl;

	cerr.setf(ios::left);
	cerr << "\nOptions:" << endl;
	cerr << setw(14) << "  -h"
	     << ": this help " << endl;
	cerr << setw(14) << "  -v"
	     << ": show version and exit" << endl;
	cerr << setw(14) << "  -t"
	     << ": test configuration and exit" << endl;
	cerr << setw(14) << "  -T"
	     << ": test configuration, dump it and exit" << endl;
	cerr << setw(14) << "  -c filename"
	     << ": configuration file path (example: examples/servio.conf)" << endl;
}

static bool _test_configuration(Config &config) {
	bool success = config.good() && config.syntaxOnly();
	cerr << NAME ": configuration file \"" << config.getPath() << "\" test is " << (success ? "successful" : "failed") << endl;
	return success;
}

static void _dump_configuration(Config &config) {
	if (_test_configuration(config)) {
		config.displayContent();
	}
}

static void _load_configuration(Config &config, const string &path) {
	struct stat buf;

	bzero(&buf, sizeof buf);
	int ret = stat(path.c_str(), &buf);
	if (S_ISDIR(buf.st_mode) || ret != 0) {
		if (S_ISDIR(buf.st_mode))
			errno = EISDIR;
		cerr << NAME ": \"" << path << "\" failed : " << strerror(errno) << endl;
	}
	config.setPath(path);
}

bool parse_options(const int &ac, char *const *av, Config &config) {
	int    opt, flags = 0;
	string opt_name, path;

	opterr = 0;  // suppress getopt error messages
	while ((opt = getopt(ac, av, "hvtTc:")) != -1) {
		switch (opt) {
		case 'h':
			flags |= HELP_OPT;
			break;
		case 'v':
			flags |= VERSION_OPT;
			break;
		case 't':
			flags |= CONF_TEST_OPT;
			break;
		case 'T':
			flags |= CONF_DUMP_OPT;
			break;
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
			cerr << NAME ": invalid option: \"" << opt_name << "\"\n";
		return false;
	} else if (flags & (VERSION_OPT | HELP_OPT | CONF_PATH_OPT | CONF_TEST_OPT | CONF_DUMP_OPT)) {
		if (flags & VERSION_OPT)
			_display_version();
		if (flags & HELP_OPT)
			_display_help();
		if (flags & CONF_PATH_OPT)
			_load_configuration(config, path);
		if (flags & CONF_TEST_OPT)
			_test_configuration(config);
		if (flags & CONF_DUMP_OPT)
			_dump_configuration(config);
		return (flags == CONF_PATH_OPT);
	}
	return true;
}