# Changelog

All notable changes to ServIO are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project tries to follow [Semantic Versioning](https://semver.org/).

## [Unreleased] — v2 branch

### Changed
- **Build switched from `-std=c++98` to `-std=c++14`.** This unlocks
  `std::unique_ptr`, lambdas, `auto`, range-for, and `= delete`. The
  project had been using `nullptr` as an Apple Clang extension under
  C++98 already; landing on a real standard with these tools is the
  bigger win.
- **`std::unique_ptr` for owned resources.** Manual `new`/`delete` is
  gone from `Body::_parser`, `Response::_sender`, `Response::_stream`,
  `Config::_asTree`, and `DefaultRouter::_chain`. The corresponding
  destructors shrank to noop or `close(fd)` only.
- **Lambdas in place of functor structs** at the four `match()` /
  `matchTo()` callsites (main.cpp, options.cpp, request.cpp,
  test_result.cpp). The anonymous-namespace structs from the C++98
  workaround are deleted.
- **`Flags<T>` adopted in `Request::_state` and `Response::_state`.**
  Raw bitmask checks like `_state & REQ_BODY` are now
  `_state.any(REQ_BODY)`; assignments like `_state = X` are
  `_state.replace(X)`. Both classes' state machines now read uniformly.
- **`auto` and range-for** at iterator-loop sites in `header.cpp`,
  `cgi.cpp`, `response.cpp`, `server.cpp`.
- `StringICaseCompare` no longer inherits from the C++17-removed
  `std::binary_function`; it carries the typedefs it needs as members.

### Added
- **State-machine helpers** in `utility/state_machine.hpp`:
  `servio::Phase<E>` for single-valued enum state with a
  self-documenting `transition()` / `is()` API, and `servio::Flags<T>`
  for bit-mask state with `enter` / `leave` / `replace` / `is` / `any` /
  `raw`. `ChunkedBodyParser` and `MultipartBodyParser` migrated to
  `Phase<>`; the former `_phase = X` raw assignments are now
  `_phase.transition(X)` and the comparisons are `_phase.is(X)`.
  9 unit tests in `test_state_machine.cpp` cover both helpers.
- **Handler chain for request routing** (Pingora-flavored). New
  `Handler` interface + `RoutingContext` + `DefaultRouter` in
  `http/router.{hpp,cpp}`. Concrete handlers — `EarlyGate`,
  `LocationGate`, `CGIDispatch`, `UploadDispatch`, `StaticServe` — each
  own one routing decision and return `Handled`/`Pass`. `Client`'s
  former `resolveResponse + tryCGI + resolveStaticFile` triplet is now
  one four-line glue method that walks the chain.
- `LineReader` — a streamsearch-backed line scanner (needle = `\n`,
  trailing `\r` stripped) used by both the request line/header parser
  and `ChunkedBodyParser`. The whole request path now goes through one
  needle-search primitive.
- `StreamSearch` — a streamsearch-inspired needle scanner used to find
  multipart boundaries across `recv()` chunks without per-byte stream reads.
- `sendfile(2)` path for static file responses (Darwin + Linux, with a
  `pread + send` fallback for other platforms). Range responses pass the
  kernel an offset directly.
- Rust-style `Option<T>` / `Result<T, E>` in `utility/result.hpp` with
  `match` / `matchTo` fold helpers and a `Unit` sentinel for void-success.
  Applied across every fallible boundary in the project:
    - `normpath(path)`           -> `Option<string>`
    - `Boundary::parse`          -> `Result<Boundary, string>`
    - `Range::parse`             -> `Result<Range, string>`
    - `Header::tryGet`           -> `Option<string>`
    - `Address::parse`           -> `Result<Address, string>`
    - `Socket::create`           -> `Result<Socket, string>`
    - `Socket::bind / listen`    -> `Result<Unit, string>`
    - `Socket::accept`           -> `Result<pair<sockfd, Address>, string>`
    - `CGI::create`              -> `Result<CGI, string>`
    - `Config::load / parse`     -> `Result<Unit, string>`
    - `servio_run`               -> `Result<Unit, string>` (top-level fold)
- The `try { ... } catch (...)` block in `main.cpp` is gone. Errors flow
  back through `servio_run`'s Result and the program exits with the right
  status via a small `match()` on the result.
- **ResponseSender Strategy** — `Response`'s body-delivery dispatch is now
  done through a `ResponseSender*` chosen at setup time:
  `LengthedSender`, `ChunkedSender`, `RangedSender`, `CGISender`,
  `UploadSender`. The 40-line `if-else-switch` in `Response::send` shrinks
  to two virtual calls.
- **Response::Builder** — fluent setup API
  (`build().status(404).keepAlive(false).contentType("text/html").body(...).apply()`).
  All `setupX` helpers now compose a Builder instead of mutating
  half a dozen fields by hand; `setupErrorResponse`'s recursive fallback
  logic is concentrated at the top and the actual response shape is one
  fluent chain.
- **Header composition over inheritance** — `Header` used to inherit
  publicly from `std::map<…>` which leaked the entire container API.
  Now it owns a private `Entries` map and exposes only the methods the
  project actually uses (`add`, `get`, `tryGet`, `setAll`, `erase`,
  iteration, …).
- **Client::handleRequest** — removed all four `goto sendResponse`
  jumps, extracted `resolveResponse(virtualServer)`, `tryCGI(location)`,
  and `resolveStaticFile(location, path)`. The top-level handler reads
  top-to-bottom and the routing decision tree is a flat sequence of
  early returns.
- **BodyParser Strategy pattern** — `LengthedBodyParser`,
  `ChunkedBodyParser`, `MultipartBodyParser` implementations of an abstract
  `BodyParser`. `Body` became a thin factory/coordinator.
- Unit-test framework under `tests/unit/` (custom, ~120 LoC, GTest-shaped
  `TEST()` macro). 60 tests covering streamsearch, body parsers, helpers,
  boundary, range, header, and Option/Result.
- End-to-end test driver under `tests/e2e/` (bash + curl). 19 tests
  covering GET, sendfile-backed range requests, multipart uploads (single /
  multi / fragmented 200KB), chunked POST, 404.
- `make test`, `make test-unit`, `make test-e2e` targets.
- OSS scaffolding: `LICENSE` (MIT), `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`,
  `SECURITY.md`, `CHANGELOG.md`, `.editorconfig`, `.clang-format`,
  GitHub issue and pull request templates, CI workflow.
- `docs/architecture.md` and `docs/grammar.md` (moved from `conf/`).
- `examples/servio.conf` and `examples/minimal.conf`.

### Changed
- Request parser is now `(const char*, size_t)`-driven instead of being fed
  from a `queue<stringstream*>`. The first-line and header parsers are a
  small byte-oriented state machine with length-bounded URI and header
  lines and CRLF/LF tolerance.
- Body parsers (lengthed / chunked / multipart) share a single
  bytes-consumed contract. Multipart writes part data straight from the
  recv buffer to per-part tmp files.
- `recv()` buffer bumped from 1 KiB to 16 KiB.
- Source files dropped the redundant `sio_` prefix (the directory already
  namespaces them); a handful were renamed for clarity
  (`http_codes` → `status_codes`, `request_body` → `body`,
  `cmdline_opts` → `options`, `utils` → `time_utils`,
  `servio.cpp` → `server.cpp`).
- Build artifacts now land in `build/` instead of polluting `src/`.

### Fixed
- Replaced the deprecated `sprintf` in `Content-Range` formatting with
  `snprintf`, restoring the `-Werror` build on recent toolchains.
- Self-referential `Body`/`Request` state no longer dangles when these
  objects are copied through `ClientMap` (`Body` is now its own
  `StreamSearch::Sink`).
- `Range: bytes=-N` (suffix range) now serves the correct last-N bytes
  instead of computing a bogus offset that closed the connection short.
- "Upload Succeffuly" typo + truncated `<h1>` in the upload response; the
  HTML status mapping was also inverted (a successful `rename` was being
  shown as "Not Uploaded"). Both are fixed.

### Changed
- Header guards modernized from reserved `__XXX_H__` form to
  `SERVIO_XXX_HPP`.
- Bit-flag `#define`s for request/response state and response type are now
  proper typed `enum`s.
- `TIMEOUT` and `BACKLOG` macros are now `static const int` in their
  natural scopes. Dead `#define CHUNK_SIZE` removed.
- **No project header carries `using namespace std;` anymore.** Every
  `.hpp` qualifies std types explicitly with `std::`. The `.cpp` files
  each take a per-file `using namespace std;` to keep implementations
  terse. Closes the cardinal C++-anti-pattern that was leaking the
  entire `std` namespace into every translation unit that included
  any project header.
- **Request parser now line-driven via `LineReader`.** The hand-coded
  byte-by-byte state machine in `Request::consume` is gone; bytes flow
  through `_lineReader.feed → takeLine`, lines dispatch through
  `parseRequestLine` / `parseHeaderLine`, and the buffer remainder hands
  off cleanly to the body parser on the empty header line.
- **ChunkedBodyParser**: 6 phases collapsed to 4
  (`ReadSizeLine`, `ReadData`, `ReadDataCRLF`, `ReadTrailerLine`). Size
  and trailer lines flow through `LineReader`; the data span stays
  byte-counted. Trailer header lines are properly skipped now (was
  best-effort before).
- **`Header::get` returns `Option<string>`.** The `""`-sentinel API is
  gone; all 9 callers migrated to `isSome`/`unwrap` or `unwrapOr`.
- **`MainContext::errorPage` returns `Option<ErrorPage*>`.** The
  `nullptr`-sentinel API is gone; the one caller in `resolveErrorBody`
  is `isNone`/`unwrap`-driven.
- **Drop `get*` prefix on accessors** across the codebase: `getPath` →
  `path`, `getQuery` → `query`, `getMethod` → `method`, `getHost` →
  `host`, `getPort` → `port`, `getSockFd` → `fd`, `getIndex` → `index`,
  `getUploadStore` → `uploadStore`, `getRedir` → `redirect`,
  `getCGIExtensions` → `cgiExtensions`, `getContentLength` →
  `contentLength`, and friends.
- **Delete dead setters**: `Address::setHost` / `setPort` (0 callsites).
- Rename ambiguous Response fields: `_lengthState` → `_rangePhase`,
  `_fd` → `_cgiFd`, `_ss` → `_headerBuffer`.
- Apply `Option::match` at the two sites it reads cleaner than
  `if (isSome) { ... }`: `Request::parseRequestLine`'s `normpath` fold,
  and `options.cpp`'s `Config::load` error sink. Other Result/Option
  sites stay on the simpler `unwrapOr` / `if isErr return` idioms.

## [1.0.0] — initial release

- HTTP/1.1 server inspired by NGINX configuration syntax.
- Static file serving, CGI, file uploads, redirects, virtual hosts, range
  requests, directory listings, custom error pages.
- `poll(2)`-driven non-blocking I/O.
