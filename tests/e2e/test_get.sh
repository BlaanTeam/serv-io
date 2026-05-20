# GET requests against static files (sendfile path).

assert_status "GET / -> 200"            200 "$(url /)"
assert_status "GET /index.html -> 200"  200 "$(url /index.html)"
assert_status "GET /anass.jpg -> 200"   200 "$(url /anass.jpg)"
assert_status "GET /missing -> 404"     404 "$(url /missing)"

# Bytes preserved exactly through sendfile.
curl -sS -o "$TMPDIR_E2E/index.html" "$(url /index.html)"
assert_md5 "GET /index.html bytes match" "$REPO_ROOT/html/index.html" "$TMPDIR_E2E/index.html"

curl -sS -o "$TMPDIR_E2E/anass.jpg" "$(url /anass.jpg)"
assert_md5 "GET /anass.jpg bytes match" "$REPO_ROOT/html/anass.jpg" "$TMPDIR_E2E/anass.jpg"

# Content-Type sniffed from extension.
ct=$(curl -sS -D - -o /dev/null "$(url /anass.jpg)" | awk '/^[Cc]ontent-[Tt]ype:/{print tolower($2)}' | tr -d '\r')
assert_eq "GET /anass.jpg Content-Type" "image/jpeg" "$ct"
