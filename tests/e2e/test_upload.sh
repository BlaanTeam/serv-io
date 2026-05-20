# Multipart file uploads.

UPLOAD_DIR="$TMPDIR_E2E/uploads"

# Single small file.
echo "single-upload-payload" > "$TMPDIR_E2E/u_small.txt"
code=$(curl -sS -o /dev/null -w "%{http_code}" -F "file=@$TMPDIR_E2E/u_small.txt;filename=u_small.txt" "$(url /upload)")
assert_eq "POST /upload small file -> 201" "201" "$code"
assert_md5 "Small upload content preserved" "$TMPDIR_E2E/u_small.txt" "$UPLOAD_DIR/u_small.txt"

# Two-file upload.
echo "alpha" > "$TMPDIR_E2E/u_a.txt"
echo "beta-content" > "$TMPDIR_E2E/u_b.txt"
code=$(curl -sS -o /dev/null -w "%{http_code}" \
    -F "a=@$TMPDIR_E2E/u_a.txt;filename=u_a.txt" \
    -F "b=@$TMPDIR_E2E/u_b.txt;filename=u_b.txt" \
    "$(url /upload)")
assert_eq "POST /upload two-file -> 201" "201" "$code"
assert_md5 "Two-file upload: a content"  "$TMPDIR_E2E/u_a.txt" "$UPLOAD_DIR/u_a.txt"
assert_md5 "Two-file upload: b content"  "$TMPDIR_E2E/u_b.txt" "$UPLOAD_DIR/u_b.txt"

# 200 KB random binary, rate-limited to force fragmented recvs.
head -c 204800 /dev/urandom > "$TMPDIR_E2E/u_big.bin"
curl -sS --limit-rate 80k -o /dev/null \
    -F "file=@$TMPDIR_E2E/u_big.bin;filename=u_big.bin" "$(url /upload)"
assert_md5 "200KB rate-limited upload content matches" "$TMPDIR_E2E/u_big.bin" "$UPLOAD_DIR/u_big.bin"
