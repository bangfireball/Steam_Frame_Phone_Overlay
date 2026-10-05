#!/bin/sh
set -eu

install_dir=${PHONECAST_INSTALL_DIR:-"$HOME/.local/opt/phonecast-vr"}
data_home=${XDG_DATA_HOME:-"$HOME/.local/share"}

rm -f "$data_home/applications/phonecast-vr.desktop"
for size in 48 128 256; do
    rm -f "$data_home/icons/hicolor/${size}x${size}/apps/phonecast-vr.png"
done
rm -rf "$install_dir"
command -v update-desktop-database >/dev/null 2>&1 && \
    update-desktop-database "$data_home/applications" >/dev/null 2>&1 || true

printf 'PhoneCast application files were removed.\n'
printf 'User settings and the pairing credential remain in ~/.config/phonecast-vr.\n'
printf 'Remove that directory separately only if you also want to forget the trusted phone.\n'
