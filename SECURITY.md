# Security policy

## Supported versions

ServIO is a research / educational web server. There is no LTS branch and no
back-porting policy. Security fixes land on `master` (and the active
development branch, if one exists).

| Version | Supported           |
|---------|---------------------|
| master  | :white_check_mark:  |
| v2.x    | :white_check_mark:  |
| < v2    | :x:                 |

## Reporting a vulnerability

**Do not** open a public GitHub issue for security-sensitive reports.

Instead, please email the maintainers at the address listed on the
repository's GitHub profile, or open a private security advisory through the
GitHub UI ("Security" tab → "Report a vulnerability"). Please include:

- A description of the issue and its impact.
- Steps to reproduce (a minimal request, configuration, or curl invocation is
  ideal).
- Any suggested fix, if you have one.

We will acknowledge receipt within a few days and aim to publish a fix and
advisory within 30 days of confirming the issue, coordinating disclosure with
the reporter.

## Known threat model

ServIO is intended for development, learning, and small private deployments.
It is **not** hardened for adversarial public internet exposure. In
particular:

- The configuration parser, CGI runner, and file upload handler operate on
  user-supplied input; you should run ServIO as an unprivileged user behind a
  trusted network or a hardened reverse proxy.
- There is no rate limiting, request smuggling defense beyond protocol
  parsing, or TLS support.

Please treat the above as a known trade-off rather than a vulnerability,
unless you find a concrete escalation (e.g., a path traversal that escapes
`root`, an upload that writes outside `upload_store`, or remote code
execution via the CGI path).
