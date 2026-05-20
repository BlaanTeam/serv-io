# shellcheck shell=bash
# Common helpers for ServIO end-to-end tests.
#
# Source this file from a test script; it expects $REPO_ROOT, $PORT,
# $TMPDIR_E2E to be set by the caller (run.sh handles all of those).

set -u

# ANSI colors (no-op if stdout is not a tty)
if [ -t 1 ]; then
    _C_OK="\033[1;32m"; _C_FAIL="\033[1;31m"; _C_DIM="\033[2m"; _C_RESET="\033[0m"
else
    _C_OK=""; _C_FAIL=""; _C_DIM=""; _C_RESET=""
fi

# Counters shared across test files when sourced from run.sh
: "${E2E_PASS:=0}"
: "${E2E_FAIL:=0}"
: "${E2E_FAILURES:=}"

pass() {
    printf "  ${_C_OK}ok  ${_C_RESET}%s\n" "$1"
    E2E_PASS=$((E2E_PASS + 1))
}

fail() {
    printf "  ${_C_FAIL}FAIL${_C_RESET} %s\n" "$1"
    [ $# -gt 1 ] && printf "      %s\n" "$2"
    E2E_FAIL=$((E2E_FAIL + 1))
    E2E_FAILURES="${E2E_FAILURES}\n  ${1}"
}

# assert_eq <description> <expected> <actual>
assert_eq() {
    local desc="$1" expected="$2" actual="$3"
    if [ "$expected" = "$actual" ]; then
        pass "$desc"
    else
        fail "$desc" "expected='$expected' actual='$actual'"
    fi
}

# assert_status <description> <expected> <url> [curl args...]
assert_status() {
    local desc="$1" expected="$2" url="$3"; shift 3
    local code
    code=$(curl -sS -o /dev/null -w "%{http_code}" "$@" "$url")
    assert_eq "$desc" "$expected" "$code"
}

# assert_md5 <description> <file_a> <file_b>
assert_md5() {
    local desc="$1" a="$2" b="$3"
    local hash_a hash_b
    if command -v md5sum >/dev/null 2>&1; then
        hash_a=$(md5sum "$a" | awk '{print $1}')
        hash_b=$(md5sum "$b" | awk '{print $1}')
    else
        hash_a=$(md5 -q "$a")
        hash_b=$(md5 -q "$b")
    fi
    assert_eq "$desc" "$hash_a" "$hash_b"
}

# `curl_to_localhost <path> [opts...]`
url() {
    printf "http://localhost:%s%s" "$PORT" "$1"
}
