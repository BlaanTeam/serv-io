# Tests

Two suites live under this directory:

```
tests/
  unit/   compiled C++ tests linked against the production sources
  e2e/    bash scripts that hit a running servio with curl/nc
```

## Running

```bash
make test         # unit + e2e
make test-unit    # unit only (fast, ~50ms)
make test-e2e    # e2e only (boots servio on port $SERVIO_TEST_PORT, default 18081)
```

CI runs both on Ubuntu and macOS — see `.github/workflows/ci.yml`.

## Unit tests (`tests/unit/`)

The unit harness is a ~120-line custom framework (`framework.{hpp,cpp}`)
that exposes a GTest-shaped `TEST(group, name)` macro and `ASSERT_*` macros.
Tests are auto-registered through a static initializer and the runner is
`tests/unit/main.cpp`. There are no external dependencies.

Adding a new test file:

1. Create `tests/unit/test_<thing>.cpp`.
2. Use `TEST(MyComponent, doesSomething) { ASSERT_EQ(...); }`.
3. `make test-unit` — the Makefile globs the directory, no list to update.

Covered today:

- `StreamSearch` — needle scanner edge cases, cross-chunk lookbehind,
  rewinding partial matches, reset semantics.
- `Boundary::parse` — Result-based Content-Type parsing.
- `Range` — closed intervals, suffix ranges, invalid input.
- `Header` — case-insensitive lookup, trimming.
- `Helpers` — `normpath` (now `Option`-returning), `trim`, `joinPath`,
  `StringICaseCompare`.
- `Option` / `Result` — Some/None, Ok/Err, `map`, `andThen`, `unwrapOr`.
- Each `BodyParser` strategy — Lengthed, Chunked, Multipart.

## End-to-end tests (`tests/e2e/`)

`run.sh` boots `servio` against `tests/e2e/servio.test.conf` (generated at
run time) on `localhost:$PORT`, sources `lib.sh` for assertion helpers, then
runs every `test_*.sh` in lexical order. The driver tears the server down on
exit.

Adding a new test:

1. Create `tests/e2e/test_<thing>.sh`. No shebang needed — `run.sh` sources
   the file into its shell.
2. Use the assertion helpers from `lib.sh`:
   - `assert_eq <desc> <expected> <actual>`
   - `assert_status <desc> <expected_code> <url>`
   - `assert_md5 <desc> <file_a> <file_b>`
   - `url <path>` for the test base URL.

Override the listening port with `SERVIO_TEST_PORT=… make test-e2e`.
