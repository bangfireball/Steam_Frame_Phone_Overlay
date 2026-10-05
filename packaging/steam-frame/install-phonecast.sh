#!/bin/sh
set -eu

bundle_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
payload_dir=$bundle_dir
if [ ! -f "$payload_dir/phonecast-vr-stream-receiver" ]; then
    payload_dir=$(dirname -- "$bundle_dir")
fi
install_dir=${PHONECAST_INSTALL_DIR:-"$HOME/.local/opt/phonecast-vr"}
applications_dir="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
icons_dir="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor"

mkdir -p "$install_dir" "$applications_dir"
for file in phonecast-vr-stream-receiver phonecast-receiver.vrmanifest \
            phonecast-actions.json phonecast-bindings-frame-controller.json \
            phonecast-bindings-generic.json; do
    if [ ! -f "$payload_dir/$file" ]; then
        echo "PhoneCast bundle is incomplete: missing $file" >&2
        exit 1
    fi
    cp -f "$payload_dir/$file" "$install_dir/$file"
done
chmod 755 "$install_dir/phonecast-vr-stream-receiver"

sed "s|@PHONECAST_INSTALL_DIR@|$install_dir|g" \
    "$bundle_dir/launch-phonecast.sh.in" >"$install_dir/launch-phonecast.sh"
cp -f "$bundle_dir/uninstall-phonecast.sh" "$install_dir/uninstall-phonecast.sh"
chmod 755 "$install_dir/launch-phonecast.sh" "$install_dir/uninstall-phonecast.sh"
sed "s|@PHONECAST_INSTALL_DIR@|$install_dir|g" \
    "$bundle_dir/phonecast-vr.desktop.in" >"$applications_dir/phonecast-vr.desktop"
chmod 644 "$applications_dir/phonecast-vr.desktop"

for size in 48 128 256; do
    destination="$icons_dir/${size}x${size}/apps"
    mkdir -p "$destination"
    cp -f "$bundle_dir/icons/${size}x${size}/phonecast-vr.png" \
        "$destination/phonecast-vr.png"
done
command -v update-desktop-database >/dev/null 2>&1 && \
    update-desktop-database "$applications_dir" >/dev/null 2>&1 || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && \
    gtk-update-icon-cache -f -t "${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor" \
        >/dev/null 2>&1 || true

printf 'PhoneCast installed in %s\n' "$install_dir"
printf 'Open the SteamVR dashboard, select +, then choose PhoneCast VR.\n'
