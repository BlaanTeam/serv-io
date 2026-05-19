# ServIO Architecture

A short tour of the major modules. For configuration grammar, see
[grammar.md](grammar.md).

```
src/
  main.cpp              entry point
  core/
    server.cpp          accept/poll loop, signal handling
    config.cpp          loads and exposes the parsed AST
    options.cpp         CLI flag parsing
    ast.cpp             configuration AST nodes
    lexer.cpp           tokenizer for the config file
    parser.cpp          recursive-descent config parser
  http/
    client.cpp          per-connection request/response dispatcher
    request.cpp         (raw-bytes) request line + header parser
    body.cpp            body parser (lengthed / chunked / multipart)
    response.cpp        response builder, sendfile path
    boundary.cpp        Content-Type boundary extraction
    streamsearch.cpp    streamsearch-style needle scanner
    header.cpp          header bag with case-insensitive keys
    range.cpp           Range request parsing
    status_codes.cpp    status code -> reason phrase
    mime_types.cpp      file extension -> MIME type
    cgi.cpp             CGI/1.1 fork+exec, env setup
  utility/
    helpers.cpp         small string + path helpers
    time_utils.cpp      timestamps, date formatting
    socket.cpp          listening socket wrapper
    logger.cpp          access / error log files
```

## Request lifecycle

```
recv()
  -> Client::handleRequest(buf, len)
       -> Request::consume(buf, len)
            -> parseRequestLine()              // first \r?\n line
            -> parseHeaderLine() (repeat)      // until empty line
            -> onHeadersComplete()
                 -> Body::chooseState(headers) // pick L/C/M parser
                 -> Body::openFile()           // tmp file for the body
            -> Body::consume(buf, len)         // streams body to disk
                  - LENGTHED   : bounded fwrite
                  - CHUNKED    : hex size + CRLF state machine
                  - MULTIPART  : StreamSearch on "\r\n--<boundary>",
                                 data emitted via Body::onData -> part file
       -> Response::setup*()                   // pick a delivery strategy
       -> Response::send(fd)
             - file body  -> sendfile(2)
             - in-memory  -> read()+send()
```

`Body` inherits `StreamSearch::Sink`, so the needle scanner can hand back
non-needle bytes via a virtual method call instead of through a back-pointer
object that would dangle on copy (see the v2 changelog).

## I/O model

A single `poll(2)` loop in `core/server.cpp`:

- Listening sockets stay readable; new connections are accepted and added
  to the poll set.
- Client sockets toggle `POLLIN` / `POLLOUT` as the request and response
  state machines advance.
- `ClientMap::purgeInactiveClients()` runs each iteration to close idle
  connections (`TIMEOUT` is in `http/response.hpp`).

The response path uses non-blocking sockets and prefers `sendfile(2)` for
regular files, which lets the kernel copy file → socket without bouncing
through userspace. macOS and Linux have different ABIs; the portable shim
lives in `http/response.cpp`.

## Configuration

The parser produces a tree of `MainContext` → `VirtualServer` → `Location`
nodes (see `core/ast.hpp`). At request time, `Config::match()` picks the
virtual server by `(address, Host header)`, and the location is matched by
longest-prefix against the request path.

For the full grammar see [grammar.md](grammar.md).
