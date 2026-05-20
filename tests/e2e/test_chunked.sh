# Transfer-Encoding: chunked end-to-end test (hand-crafted on the wire).

response=$(printf 'POST / HTTP/1.1\r\nHost: localhost:%s\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n' "$PORT" \
    | nc -w 2 localhost "$PORT" | head -1)
case "$response" in
    "HTTP/1.1 200 OK"*)  pass "chunked POST accepted" ;;
    *)                   fail "chunked POST accepted" "got: $response" ;;
esac
