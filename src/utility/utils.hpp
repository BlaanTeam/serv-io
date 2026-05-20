#ifndef SERVIO_UTILS_HPP
#define SERVIO_UTILS_HPP

#include <sys/time.h>

#include <iostream>

#include "core/ast.hpp"
#include "utility/helpers.hpp"

void dumpConfigDot(MainContext<> *main, std::ostream &stream = std::cout);

std::string getUTCDate(void);
long long   getmstime(void);

#endif
