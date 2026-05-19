# Contributing to ServIO

Thanks for your interest in improving ServIO! This document explains how to
get a development environment running, the conventions we follow, and how to
submit changes.

## Quick start

```bash
git clone https://github.com/BlaanTeam/serv-io.git
cd serv-io
make            # builds ./servio with debug symbols + AddressSanitizer
./servio -c examples/servio.conf
```

## Project layout

```
src/             Source code (entry point + core/http/utility modules)
docs/            Architecture notes, configuration grammar
examples/        Sample configuration files
html/            Default document root served by the example config
.github/         CI workflows, issue and PR templates
```

See [docs/architecture.md](docs/architecture.md) for a tour of the modules.

## Building

| Target              | Result                                                |
|---------------------|-------------------------------------------------------|
| `make` / `make all` | Debug build with `-ggdb` and AddressSanitizer         |
| `make release`      | Optimized build (`-O2`, no sanitizer)                 |
| `make asan`         | Explicit ASan build (alias of the default)            |
| `make clean`        | Remove object files and the `build/` directory        |
| `make fclean`       | Remove all build artifacts including the binary       |
| `make re`           | `fclean` + `all`                                      |
| `make format`       | Run `clang-format -i` over `src/` (requires the tool) |
| `make install`      | Install `servio` to `$(DESTDIR)$(PREFIX)/bin`         |

`PREFIX` defaults to `/usr/local` and can be overridden:

```bash
make install PREFIX=$HOME/.local
```

## Coding style

- C++98 — no `<thread>`, `<chrono>`, range-based for, `auto`, etc. Apple Clang
  accepts `nullptr` as an extension and we use it; nothing else from C++11+.
- Indent with **tabs**, width 4. Continuations align on spaces. The repository
  ships a `.clang-format`; run `make format` before opening a PR.
- Class names: `UpperCamelCase` (`Request`, `StreamSearch`).
- Method names: `lowerCamelCase` (`consume`, `parseHeaderLine`).
- Members: leading underscore + `lowerCamelCase` (`_bodyState`).
- Source files: `lower_snake_case.{cpp,hpp,tpp}`; directory namespaces the
  file (no extra `sio_` prefix).
- Prefer named constants to magic numbers; bit-mask state machines use
  `(1 << N)` `#define`s for compactness.

## Tests

Run the smoke checks in `tests/` (if present) before submitting changes:

```bash
make re
./servio -t -c examples/servio.conf   # syntax check the config
./servio -c examples/servio.conf &    # start
curl -i http://localhost:8081/
curl -F file=@README.md http://localhost:8081/upload
```

CI (`.github/workflows/ci.yml`) compiles on Linux and macOS and runs the same
smoke checks — keep it green.

## Pull requests

1. Branch off `master` (or `v2` while v2 is in development).
2. Make the commit messages descriptive; the repository uses Conventional
   Commits style (`feat:`, `fix:`, `refactor:`, `chore:`, `docs:` …).
3. Open a PR against the appropriate base branch. The PR template prompts you
   for a summary and test plan.
4. Be ready to discuss design — small, focused PRs are easier to land.

## Reporting issues

Please use the issue templates in `.github/ISSUE_TEMPLATE/`:

- **Bug report** — include reproduction steps, expected vs. actual behavior,
  and the relevant section of your configuration file.
- **Feature request** — describe the use case and any prior art in NGINX
  (since the config grammar follows NGINX).

For anything that looks like a security vulnerability, see
[SECURITY.md](SECURITY.md) instead of opening a public issue.

## Code of conduct

All contributors are expected to follow the
[Contributor Covenant](CODE_OF_CONDUCT.md).
