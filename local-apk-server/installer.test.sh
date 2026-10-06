#!/bin/sh
# Bootstrap tests use fake platform/network commands and a harmless fixture,
# never the real receiver or the real installation directory.
set -eu
script=$1
sandbox=$(mktemp -d)
trap 'rm -rf "$sandbox"' EXIT
mkdir -p "$sandbox/bin" "$sandbox/tmp" "$sandbox/payload/phonecast-steam-frame-arm64-sprint14/steam-frame-installer"
cat > "$sandbox/bin/uname" <<'EOF'
#!/bin/sh
case "$1" in -s) echo Linux;; -m) echo "${TEST_ARCH:-aarch64}";; esac
EOF
cat > "$sandbox/bin/id" <<'EOF'
#!/bin/sh
echo "${TEST_UID:-1000}"
EOF
cat > "$sandbox/bin/curl" <<'EOF'
#!/bin/sh
[ "${TEST_DOWNLOAD_FAIL:-0}" = 0 ] || exit 22
while [ "$#" -gt 0 ]; do
  if [ "$1" = --output ]; then cp "$TEST_ARCHIVE" "$2"; exit; fi
  shift
done
exit 1
EOF
cat > "$sandbox/payload/phonecast-steam-frame-arm64-sprint14/steam-frame-installer/install-phonecast.sh" <<'EOF'
#!/bin/sh
printf 'installed\n' > "$TEST_MARKER"
exit "${TEST_INSTALL_STATUS:-0}"
EOF
chmod +x "$sandbox/bin/"*
tar -czf "$sandbox/bundle.tar.gz" -C "$sandbox/payload" .
export PATH="$sandbox/bin:$PATH" TMPDIR="$sandbox/tmp"
export TEST_ARCHIVE="$sandbox/bundle.tar.gz" TEST_MARKER="$sandbox/installed"
sh "$script" http://trusted.example:8080
[ -f "$TEST_MARKER" ]
[ -z "$(ls -A "$TMPDIR")" ]
rm "$TEST_MARKER"
expect_failure() {
  if sh "$script" "$@"; then echo 'Expected bootstrap failure' >&2; exit 1; fi
  [ -z "$(ls -A "$TMPDIR")" ]
}
export TEST_ARCH=x86_64
expect_failure http://trusted.example:8080
unset TEST_ARCH
export TEST_UID=0
expect_failure http://trusted.example:8080
unset TEST_UID
expect_failure
expect_failure file:///tmp
export TEST_DOWNLOAD_FAIL=1
expect_failure http://trusted.example:8080
[ ! -f "$TEST_MARKER" ]
unset TEST_DOWNLOAD_FAIL
export TEST_INSTALL_STATUS=9
expect_failure http://trusted.example:8080
rm "$TEST_MARKER"
unset TEST_INSTALL_STATUS
printf 'invalid archive' > "$TEST_ARCHIVE"
expect_failure http://trusted.example:8080
[ ! -f "$TEST_MARKER" ]
rm -rf "$sandbox/payload/phonecast-steam-frame-arm64-sprint14"
tar -czf "$TEST_ARCHIVE" -C "$sandbox/payload" .
expect_failure http://trusted.example:8080
[ ! -f "$TEST_MARKER" ]
echo 'Bootstrap success, platform/root checks, failures and cleanup passed.'
