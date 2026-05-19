#ifndef __CMDLINE_OPTIONS_H__
#define __CMDLINE_OPTIONS_H__

#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iomanip>

#include "./config.hpp"


class Config;

enum OptionTypes {
	HELP_OPT = (1 << 1),
	VERSION_OPT = (1 << 2),
	CONF_TEST_OPT = (1 << 3),
	CONF_DUMP_OPT = (1 << 4),
	CONF_PATH_OPT = (1 << 5),
	UNKNOWN_OPT = (1 << 6)
};

bool parse_options(const int &ac, char *const *av, Config &config);

#endif