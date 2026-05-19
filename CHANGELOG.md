# Changelog

All notable changes to ServIO are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project tries to follow [Semantic Versioning](https://semver.org/).

## [Unreleased] — v2 branch

### Added
- `StreamSearch` — a streamsearch-inspired needle scanner used to find
  multipart boundaries across `recv()` chunks without per-byte stream reads.
- `sendfile(2)` path for static file responses (Darwin + Linux, with a
  `pread + send` fallback for other platforms). Range responses pass the
  kernel an offset directly.
- OSS scaffolding: `LICENSE` (MIT), `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`,
  `SECURITY.md`, `CHANGELOG.md`, `.editorconfig`, `.clang-format`,
  GitHub issue and pull request templates, CI workflow.
- `docs/architecture.md` and `docs/grammar.md` (moved from `conf/`).
- `examples/servio.conf` (sample configuration).

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

## [1.0.0] — initial release

- HTTP/1.1 server inspired by NGINX configuration syntax.
- Static file serving, CGI, file uploads, redirects, virtual hosts, range
  requests, directory listings, custom error pages.
- `poll(2)`-driven non-blocking I/O.
