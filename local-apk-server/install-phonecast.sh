#!/bin/sh
# Trusted-LAN development bootstrap; run as the normal Steam Frame user.
set -eu

fail() { printf '%s\n' "$*" >&2; exit 1; }
[ "$(uname -s)" = Linux ] || fail 'PhoneCast requires Linux on Steam Frame.'
case "$(uname -m)" in
    aarch64|arm64) ;;
    *) fail 'PhoneCast requires an ARM64 headset; do not install this bundle on your PC.' ;;
esac
[ "$(id -u)" != 0 ] || fail 'Run as your normal headset user, without sudo.'
[ "$#" -eq 1 ] || fail 'Usage: sh install-phonecast.sh http://YOUR_PC_LAN_IP:8080'
base_url=${1%/}
case "$base_url" in
    http://*|https://*) ;;
    *) fail 'The download server address must start with http:// or https://.' ;;
esac
for tool in curl tar mktemp; do
    command -v "$tool" >/dev/null 2>&1 || fail "Required command not found: $tool"
done

printf '%s\n' 'Install only from your trusted download server. Quit PhoneCast before updating.'
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
curl --fail --show-error --location --connect-timeout 15 \
    --output "$work_dir/phonecast.tar.gz" \
    "$base_url/phonecast-steam-frame-arm64.tar.gz"
tar -xzf "$work_dir/phonecast.tar.gz" -C "$work_dir"
installer="$work_dir/phonecast-vr-v0.8-steam-frame-arm64/steam-frame-installer/install-phonecast.sh"
[ -f "$installer" ] || fail 'Downloaded bundle is missing the PhoneCast installer.'
sh "$installer"
