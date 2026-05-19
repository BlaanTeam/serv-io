#ifndef __SERVIO_H__
#define __SERVIO_H__

#include <map>
#include <set>

#include "./options.hpp"
#include "./config.hpp"
#include "http/client.hpp"
#include "utility/socket.hpp"

using namespace std;

void servio_init(const int &ac, char *const *av);
void handleSignals(void);

#endif