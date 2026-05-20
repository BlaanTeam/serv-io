#!/usr/bin/env bash
# End-to-end test driver. Boots ServIO, runs all tests/e2e/test_*.sh against
# it, then shuts the server down. Exits non-zero if any test failed.

set -u

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

PORT=${SERVIO_TEST_PORT:-18081}
CONF="$REPO_ROOT/tests/e2e/servio.test.conf"
TMPDIR_E2E="$(mktemp -d /tmp/servio_e2e.XXXXXX)"
SERVER_LOG="$TMPDIR_E2E/server.log"
SERVER_PID=

cleanup() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$TMPDIR_E2E"
}
trap cleanup EXIT

# Write a self-contained config that maps the test port to our html/ root and
# allows uploads into the per-run tmpdir.
mkdir -p "$TMPDIR_E2E/uploads"
cat >"$CONF" <<EOF
http {
    client_max_body_size 30m;

    server {
        listen $PORT;

        location / {
            root html;
            index index.html;
            autoindex on;
        }

        location /upload {
            allowed_methods POST;
            upload_store $TMPDIR_E2E/uploads;
        }
    }
}
EOF

if [ ! -x "$REPO_ROOT/servio" ]; then
    echo "servio binary not built — run 'make' first" >&2
    exit 1
fi

"$REPO_ROOT/servio" -t -c "$CONF" >/dev/null 2>&1 || {
    echo "config $CONF failed syntax check" >&2
    exit 1
}

"$REPO_ROOT/servio" -c "$CONF" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!

# Wait for the server to start listening.
for _ in 1 2 3 4 5 6 7 8 9 10; do
    if curl -sS -o /dev/null --connect-timeout 1 "http://localhost:$PORT/" 2>/dev/null; then
        break
    fi
    sleep 0.1
done

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "server failed to start; log follows:" >&2
    cat "$SERVER_LOG" >&2
    exit 1
fi

# shellcheck disable=SC1091
. "$REPO_ROOT/tests/e2e/lib.sh"

printf "${_C_DIM}running e2e tests against http://localhost:%s${_C_RESET}\n\n" "$PORT"

for tf in "$REPO_ROOT"/tests/e2e/test_*.sh; do
    [ -f "$tf" ] || continue
    # shellcheck disable=SC1090
    . "$tf"
done

echo
if [ "$E2E_FAIL" -eq 0 ]; then
    printf "${_C_OK}%d passed${_C_RESET}, 0 failed\n" "$E2E_PASS"
    exit 0
else
    printf "%d passed, ${_C_FAIL}%d failed${_C_RESET}\n" "$E2E_PASS" "$E2E_FAIL"
    exit 1
fi
