# ServIO

A lightweight, high-performance HTTP web server implemented in C++98, inspired by NGINX configuration syntax and designed for modern web applications.

## Overview

ServIO is a custom HTTP/1.1 web server that provides essential web server functionality including static file serving, CGI support, file uploads, redirections, and virtual host management. The server uses a configuration system similar to NGINX, making it familiar and easy to configure.

## Features

- **HTTP/1.1 Protocol Support** - Full implementation of HTTP/1.1 with persistent connections
- **Virtual Hosts** - Multiple server blocks with different ports and server names
- **Static File Serving** - Efficient serving of static content with MIME type detection
- **CGI Support** - Execute server-side scripts (Python, PHP, etc.)
- **File Upload** - Handle file uploads with configurable storage locations
- **URL Redirections** - Support for HTTP redirects (301, 302, etc.)
- **Directory Indexing** - Automatic directory listing when enabled
- **Error Pages** - Customizable error pages for different HTTP status codes
- **Request Size Limits** - Configurable maximum request body size
- **Method Restrictions** - Control allowed HTTP methods per location
- **Polling I/O** - Non-blocking I/O using poll() for high concurrency
- **Signal Handling** - Proper signal management for graceful operation

## Architecture

The server is built with a modular architecture:

- **Core** (`src/core/`) - Main server logic, configuration parsing, and AST
- **HTTP** (`src/http/`) - HTTP protocol implementation, request/response handling
- **Utility** (`src/utility/`) - Socket management, logging, and helper functions

### Key Components

- **Configuration Parser** - Lexer and parser for NGINX-like configuration syntax
- **Socket Manager** - Non-blocking socket operations with poll()
- **Client Manager** - Connection handling and lifecycle management
- **Request Processor** - HTTP request parsing and validation
- **Response Generator** - HTTP response construction and delivery
- **CGI Handler** - External script execution and communication

## Build Requirements

- **Compiler**: C++ compiler with C++98 standard support
- **Build System**: GNU Make
- **Platform**: Unix-like systems (Linux, macOS)

## Installation

1. Clone the repository:
```bash
git clone <repository-url>
cd servIO
```

2. Build the server:
```bash
make
```

3. Clean build artifacts:
```bash
make clean      # Remove object files
make fclean     # Remove all build artifacts
make re         # Clean rebuild
```

## Configuration

ServIO uses a configuration file syntax similar to NGINX. The default configuration is located at `conf/servio.conf`.

### Basic Configuration Structure

```nginx
http {
    client_max_body_size 30m;

    server {
        listen 80;
        server_name example.com;

        location / {
            root html;
            index index.html;
        }

        location /cgi {
            cgi_assign ".py";
        }

        location /upload {
            upload_store /tmp;
        }
    }
}
```

### Configuration Directives

#### HTTP Block Directives
- `client_max_body_size` - Maximum size of client request body
- `root` - Document root directory
- `allowed_methods` - Allowed HTTP methods (GET, POST, DELETE)
- `autoindex` - Enable/disable directory listing
- `index` - Default index files
- `error_page` - Custom error page mapping

#### Server Block Directives
- `listen` - Port to listen on
- `server_name` - Virtual host server name
- `return` - HTTP redirect response

#### Location Block Directives
- `cgi_assign` - File extensions for CGI execution
- `upload_store` - Directory for file uploads
- `return` - Location-specific redirects

### Configuration Grammar

The complete BNF grammar for the configuration file is documented in `conf/grammar.md`.

## Usage

1. **Start the server** with default configuration:
```bash
./servio
```

2. **Use custom configuration**:
```bash
./servio -c /path/to/config.conf
```

3. **Check configuration syntax**:
```bash
./servio -t
```

The server will start listening on the configured ports and serve content according to the configuration directives.

## Example Configurations

### Static File Server
```nginx
http {
    server {
        listen 8080;
        location / {
            root /var/www/html;
            autoindex on;
        }
    }
}
```

### CGI Application Server
```nginx
http {
    server {
        listen 80;
        location / {
            root html;
        }
        location /cgi-bin {
            cgi_assign ".py" ".php";
        }
    }
}
```

### File Upload Server
```nginx
http {
    client_max_body_size 100m;
    server {
        listen 80;
        location /upload {
            allowed_methods POST;
            upload_store /var/uploads;
        }
    }
}
```

## Project Structure

```
servIO/
├── src/
│   ├── core/           # Core server functionality
│   ├── http/           # HTTP protocol implementation
│   ├── utility/        # Utility functions and helpers
│   └── sio_main.cpp    # Main entry point
├── conf/
│   ├── servio.conf     # Default configuration
│   └── grammar.md      # Configuration grammar documentation
├── html/               # Default web content
├── logs/               # Log files
├── Makefile           # Build configuration
└── .gitignore         # Git ignore rules
```

## Development

### Code Style
- C++98 standard compliance
- RAII principles for resource management
- Error handling with exceptions where appropriate
- Memory leak prevention with proper cleanup

### Debugging
The server is built with debugging symbols (`-ggdb`) and includes AddressSanitizer support for memory error detection.

### Testing
The configuration includes test locations for validating various server features:
- Method restrictions (`/tests/allow`)
- Body size limits (`/tests/max_size`)
- CGI execution (`/cgi`)
- File uploads (`/upload`)
- Redirections (`/redirect`)

## Contributing

1. Follow the existing code style and C++98 standards
2. Test your changes with various configuration scenarios
3. Ensure memory safety and proper error handling
4. Update documentation for new features

## License

This project is provided as-is for educational and development purposes.