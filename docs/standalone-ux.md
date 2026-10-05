# Steam Frame standalone launch and lifecycle

## Owner-accepted Sprint 12 baseline

Sprint 12 was closed by the project owner on 2026-10-05 after physical use and
an issue-free video playback session corroborated by a healthy log review.
Closure accepts the manual-launch baseline, not every extended lifecycle case.
Known freeze recovery and untested cases remain explicit follow-ups below.

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
opens the PhoneCast dashboard. The project owner physically confirmed dashboard
`+` discovery, first launch, and duplicate-launch prevention. Explicit
second-launch focus and dashboard recreation remain separate physical checks.

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

## Deferred follow-ups after owner-approved closure

- enabling/disabling OpenVR autostart from PhoneCast;
- automatic LAN discovery;
- encrypted per-device credentials and revocation;
- explicit second-launch focus and dashboard recreation checks;
- verified outcomes for SteamVR restart, headset sleep/wake, broader game
  switching, receiver crash, and game-first standalone-VR-scene launch;
- physical use of the new curl bootstrap (automated fixture tests pass);
- investigation/recovery for the earlier video freezes and V4L2 busy failure.

PhoneCast-first coexistence with standalone Cubism is physically confirmed.
Autostart remains unimplemented/off; closure does not authorize enabling it.

## Closure log evidence — 2026-10-05

Read-only SSH review of approximately 1,000 new diagnostics samples found no new
V4L2 busy error, Vulkan submission error, or sustained incoming-video-without-
decode failure. The 599 dynamic samples averaged 29.57 receive FPS, 29.51 decode
FPS, 29.13 submitted FPS, 1.62% process CPU, and 105.54 MiB working set (dynamic
range 104.53–106.09 MiB). Logs show portrait/landscape texture recreation,
reconnect, and intentional dashboard Quit/relaunch.

The latest CSV covers 169 seconds and reached first submission in 88.47 ms.
Its four drops/two resyncs were confined to startup/rotation samples; its final
minute plateaued at 105.19 MiB memory. Four samples in the broader appended log
reported a mispresented-frame snapshot with temporary submission-cost spikes;
the latest CSV had none and the user reported no issue. These snapshots are not
a complete compositor failure count or controlled performance comparison.

The earlier download-associated and Steam-purchase-session freezes remain open;
this successful session does not establish a fix. See `design.md` and
[`steam-frame.md`](steam-frame.md) for their separate evidence and limitations.
Windows build/all 11 CTest tests and both download-server/bootstrap tests passed
at closure. No native code changed or new ARM64 build was claimed by this review.

## Related Frametop research

A source review of the native Steam Frame desktop project Frametop is recorded
in [`frametop-research.md`](frametop-research.md). It reinforces the rule that a
helper must not accidentally start `vrserver`: Frametop probes first as an
OpenVR background application and couples relevant services to
`steamvr.service` before initializing as an overlay. PhoneCast's launcher
already refuses to start while SteamVR is intentionally stopped; the two-stage
probe is optional future defense-in-depth, not a Sprint 12 acceptance item.

Frametop's DMA-BUF import path and aim-gated dashboard-closed interaction are
follow-up optimization and Sprint 14 research leads. They do not replace the
current Vulkan renderer, dashboard-first controls, application manifest,
single-instance IPC, pairing persistence, or any physical lifecycle test in
this document.
