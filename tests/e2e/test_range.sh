# HTTP Range requests via sendfile.

# Closed interval.
code=$(curl -sS -o "$TMPDIR_E2E/range_a.bin" -H "Range: bytes=100-199" -w "%{http_code}" "$(url /anass.jpg)")
assert_eq "Range bytes=100-199 status" "206" "$code"
size=$(wc -c < "$TMPDIR_E2E/range_a.bin" | tr -d ' ')
assert_eq "Range bytes=100-199 length"  "100" "$size"

# Open suffix (last N bytes).
code=$(curl -sS -o "$TMPDIR_E2E/range_b.bin" -H "Range: bytes=-50" -w "%{http_code}" "$(url /anass.jpg)")
assert_eq "Range bytes=-50 status" "206" "$code"
size=$(wc -c < "$TMPDIR_E2E/range_b.bin" | tr -d ' ')
assert_eq "Range bytes=-50 length" "50"  "$size"

# Content-Range header is present.
hdr=$(curl -sS -D - -o /dev/null -H "Range: bytes=0-9" "$(url /anass.jpg)" | awk '/^[Cc]ontent-[Rr]ange:/{print $2 $3 $4}' | tr -d '\r')
[ -n "$hdr" ] && pass "Range responses include Content-Range" || fail "Range responses include Content-Range" "missing header"
