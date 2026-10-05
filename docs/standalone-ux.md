# Steam Frame standalone launch and lifecycle

## Implemented Sprint 12 baseline

PhoneCast now ships a supported manual-launch package for Steam Frame. The
manual path intentionally precedes optional autostart.

The native bundle contains `steam-frame-installer/install-phonecast.sh`. Running
it on the headset installs the receiver under
`~/.local/opt/phonecast-vr` by default and creates:

- `~/.local/share/applications/phonecast-vr.desktop`;
- 48, 128, and 256 pixel icons in the user hicolor icon theme;
- an absolute-path launcher that refuses to start SteamVR when the runtime is
  intentionally stopped;
- the executable, OpenVR application manifest, and input manifests with the
  executable bit preserved.

Set `PHONECAST_INSTALL_DIR` while installing or uninstalling to use a different
absolute installation directory. The generated desktop entry is rewritten for
that location.

After installation, open the SteamVR dashboard, choose **+**, and select
**PhoneCast VR**. The launcher starts the native receiver with
`--focus-dashboard`. The receiver registers its adjacent OpenVR manifest and
opens the PhoneCast dashboard. This needs physical confirmation on Steam Frame;
a successful cross-build does not establish launcher visibility.

## Single instance and focus

The native process takes a per-user `flock` under `$XDG_RUNTIME_DIR` (or a
private `/tmp/phonecast-vr-UID` fallback) and listens on an owner-only Unix-domain
socket. A second launch does not initialize OpenVR, open the video decoder, or
bind the streaming port. It sends `SHOW_DASHBOARD` to the resident process and
exits successfully. The resident process asks OpenVR to show the dashboard with
the PhoneCast tab selected.

The OpenVR backend checks its dashboard handle every two seconds. If SteamVR
drops that handle while the process and runtime remain available, PhoneCast
recreates the dashboard main/thumbnail pair. Full SteamVR process restart and
headset sleep/wake behavior still require physical testing; this health check
must not be represented as proof of those cases.

## Pairing and reconnect

On first native launch, PhoneCast creates a random six-digit credential at
`$XDG_CONFIG_HOME/phonecast-vr/pairing-code`, or
`~/.config/phonecast-vr/pairing-code`, with owner-only permissions. It is reused
on later launches and shown inside the PhoneCast dashboard rather than written
to the receiver log. The Android sender already persists the manually entered
receiver address and pairing code and automatically reconnects while its capture
session remains active.

This is persistence for the existing pairing gate, not encrypted device
identity. The stream remains restricted to a trusted LAN. Automatic discovery
and cryptographic device authentication are not implemented by this baseline;
manual receiver-address entry remains available.

To rotate the pairing credential, stop PhoneCast, remove the `pairing-code`
file, and launch it again. The Android sender must then be updated with the new
code.

## Logs and removal

The desktop launcher writes:

- receiver output to `$XDG_STATE_HOME/phonecast-vr/receiver.log` (or
  `~/.local/state/phonecast-vr/receiver.log`);
- rolling performance samples to the adjacent `performance.csv`.

Run `~/.local/opt/phonecast-vr/uninstall-phonecast.sh` after intentionally
quitting PhoneCast. It removes application files, desktop entry, and icons, but
preserves `~/.config/phonecast-vr` so updates and reinstalls do not silently
forget settings or pairing. Remove that configuration directory separately only
when the phone should be forgotten.

## Deferred until physical launcher validation

- enabling/disabling OpenVR autostart from PhoneCast;
- automatic LAN discovery;
- encrypted per-device credentials and revocation;
- verified outcomes for SteamVR restart, headset sleep/wake, game switching,
  receiver crash, and both standalone-VR-scene launch orders.

Autostart must remain off until the manual dashboard `+` launch and recovery
path has passed physical testing.
