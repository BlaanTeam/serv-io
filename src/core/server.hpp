#ifndef SERVIO_SERVER_HPP
#define SERVIO_SERVER_HPP

#include <map>
#include <set>

#include "./config.hpp"
#include "./options.hpp"
#include "http/client.hpp"
#include "utility/result.hpp"
#include "utility/socket.hpp"

// Top-level entry point. Returns Ok(()) on a clean exit (or after handling
// `-h`/`-v`), Err(msg) on startup failure. The loop itself is currently
// infinite, so a successful Ok return only happens for the early-exit
// command-line flags.
servio::Result<servio::Unit, std::string> servio_run(const int &ac, char *const *av);

void handleSignals(void);

#endif
