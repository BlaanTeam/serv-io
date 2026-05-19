---
name: Bug report
about: Report something that is not working as expected
labels: bug
---

## Description

A clear, concise description of what the bug is.

## Steps to reproduce

1. Run `./servio -c ...`
2. Issue request `curl ...`
3. Observe `<actual behavior>`

## Expected behavior

What you expected to happen instead.

## Environment

- OS / version (e.g. Ubuntu 22.04, macOS 14.4)
- Compiler / version (`c++ --version`)
- ServIO commit (`git rev-parse HEAD`)
- Build flags (default debug+ASan? `make release`? other?)

## Relevant configuration

```nginx
# paste the smallest config snippet that reproduces the issue
```

## Logs / output

```
# server stderr, curl -v output, ASan/UBSan reports, etc.
```
