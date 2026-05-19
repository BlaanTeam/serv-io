# ServIO

[![ci](https://github.com/BlaanTeam/serv-io/actions/workflows/ci.yml/badge.svg)](https://github.com/BlaanTeam/serv-io/actions/workflows/ci.yml)
[![license: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++98](https://img.shields.io/badge/C%2B%2B-98-informational.svg)](#building)

A lightweight HTTP/1.1 web server written in C++98, inspired by NGINX's
configuration syntax. ServIO does static file serving, CGI, file uploads,
range requests, virtual hosts, and custom error pages — all driven by a
non-blocking `poll(2)` loop and a streaming request parser.

## Features

- HTTP/1.1 with persistent connections and `Range:` support
- Virtual hosts (per listening port / `Host` header)
- Static file serving via `sendfile(2)` on Linux and macOS
- CGI/1.1 (fork+exec, configurable script extensions)
- Multipart file uploads, decoded with a streamsearch-style needle scanner
- Configurable error pages, redirects, directory listings, body-size limits
- NGINX-flavoured configuration with a small recursive-descent parser

## Quick start

```bash
git clone https://github.com/BlaanTeam/serv-io.git
cd serv-io
make                                  # debug + AddressSanitizer
./servio -t -c examples/minimal.conf  # syntax-check the example config
./servio   -c examples/minimal.conf   # listen on http://localhost:8081
```

Then:

```bash
curl -i http://localhost:8081/
echo "hello" > /tmp/up.txt && curl -F file=@/tmp/up.txt http://localhost:8081/upload
```

## Building

| Target              | Purpose                                                    |
|---------------------|------------------------------------------------------------|
| `make` / `make all` | Debug build with `-ggdb` and AddressSanitizer (default).   |
| `make release`      | Optimized build (`-O2`, no sanitizer).                     |
| `make clean`        | Remove `build/`.                                           |
| `make fclean`       | Remove all build artifacts including the binary.           |
| `make re`           | `fclean` + `all`.                                          |
| `make format`       | Run `clang-format -i` on `src/` using the project style.   |
| `make install`      | Install `servio` to `$(DESTDIR)$(PREFIX)/bin` (`/usr/local`). |

The build requires a C++ compiler that accepts `-std=c++98` (Apple Clang,
GCC, recent Clang) and GNU Make. No third-party dependencies.

## Usage

```
Usage: servio [-hvtT] [-c filename]

  -h            this help
  -v            show version and exit
  -t            test configuration and exit
  -T            test configuration, dump it, and exit
  -c filename   configuration file path (example: examples/servio.conf)
```

## Configuration

ServIO uses an NGINX-style configuration file. A minimal example:

```nginx
http {
    client_max_body_size 30m;

    server {
        listen 8081;

        location / {
            root html;
            index index.html;
            autoindex on;
        }

        location /upload {
            allowed_methods POST;
            upload_store /tmp;
        }
    }
}
```

Full grammar: [docs/grammar.md](docs/grammar.md).
Example configs: [examples/](examples/).

### Directive cheatsheet

| Block      | Directive                | Notes                                                  |
|------------|--------------------------|--------------------------------------------------------|
| `http`     | `client_max_body_size`   | Maximum request body, e.g. `30m`                       |
|            | `root`                   | Default document root                                  |
|            | `allowed_methods`        | `GET` `POST` `DELETE` (whitelist)                      |
|            | `autoindex on\|off`      | Directory listing fallback                             |
|            | `index <files>...`       | Index files searched in order                          |
|            | `error_page <code> <path>`| Custom error page                                     |
| `server`   | `listen <port>`          | Listening port (one per server block)                  |
|            | `server_name <hosts>...` | `Host` header match                                    |
|            | `return <code> <target>` | Redirect / static response                             |
| `location` | `cgi_assign <exts>...`   | Run scripts matching extensions as CGI                 |
|            | `upload_store <dir>`     | Where multipart uploads land                           |

## Project layout

```
src/             Source code (entry point + core/http/utility modules)
docs/            Architecture notes and config grammar
examples/        Sample configuration files
html/            Default document root used by the example config
.github/         CI workflow and issue/PR templates
```

See [docs/architecture.md](docs/architecture.md) for a tour of the modules.

## Contributing

PRs and issues welcome — see [CONTRIBUTING.md](CONTRIBUTING.md) and
[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). For security reports, see
[SECURITY.md](SECURITY.md).

## License

[MIT](LICENSE).
