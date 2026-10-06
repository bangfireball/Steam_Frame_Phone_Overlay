# Sprint 14 working handoff — 2026-10-06

## Current request and scope

The owner requested a handoff **while work is ongoing**. Sprint 14 remains open. The latest response was “that's better” after the adjustable Start Location build. Record that as positive placement feedback, not full approval of the settings preview, gestures, shortcuts, drag depth, lifecycle, or Sprint 14 acceptance criteria.

Continue Sprint 14 only. Sprint 12 native decoder-fault reproduction and Vulkan device-loss recovery remain separate deferred tasks. Do not begin Sprint 15 or reopen owner-approved Sprint 13 audio work without new evidence/request.

Before implementation, read `design.md` completely, `docs/development.md`, and relevant subsystem documentation. Preserve Core/VR/platform boundaries and the native Steam Frame ARM64 target.

## Repository state

- Branch: `main`.
- Clean working tree at handoff preparation, before adding this documentation.
- Six implementation/publication commits ahead of `origin/main`; not pushed.
- Latest implementation commit: `513147e` — adjustable user-facing start location with preview.
- Earlier session commits:
  - `17482d2` — either-stick hold/double shortcuts and explicit panel-depth axes;
  - `10ee1bc` — world reset, movement/docking corrections, first narrow left-stick shortcut;
  - `d9416f5` — landscape quick controls and initial phone resize corner;
  - `4d8f418` — serve Sprint 14 bundle;
  - `32ce13d` — initial mockup-driven UI refresh.
- `_injest/` is ignored in `.gitignore`. References are `_injest/mockup-quick-panel.png` and `_injest/mockup-settings-panel.png`. Do not package or commit the images without a new request.

## Physical feedback chronology

1. Initial Pin and Glance actions had no useful visible distinction. Pin was identical to ordinary Expanded; Glance only made a smaller controller attachment. Removed both from the primary dashboard, retaining internal compatibility states.
2. Initial Show did not reliably reveal the phone until Head/World was selected. Owner requested a reveal next to the quick UI and a landscape quick panel.
3. First resize corner physically worked, but movement/docking regressed. Close beside Back was rejected. Owner requested world-space reset rather than head lock, no dashboard Back, left-stick hold toggle, and move-handle stick push/pull.
4. World reset was visible but too small/low; left-stick hold worked after multiple tries; legacy stick push/pull did **not** work. Revised reveal sizing/position, added both-stick hold and double-click docking, and replaced legacy axis reads with explicit actions. These revised shortcuts/axes have not been individually physically approved.
5. Next Show was better but too far right and not facing the user. Owner requested notification-style placement controls with dummy preview.
6. Latest adjustable Start Location build received “that's better.” No detailed preview/apply/cancel/persistence or drag-depth/shortcut approval was stated.
7. A dashboard-active hotkey experiment physically failed: neither hold nor double-click actions delivered while the SteamVR dashboard owned focus. The shortcut set is again disabled there. Repeated same-hand double-click world pinning was then implemented and physically reported as perfect.
8. Start Location settings/preview and the phone footer's Back, Close, and Resize behavior/appearance were physically approved in normal use. Explicit-action joystick depth still does not work while the dashboard owns focus and now requires input-route research rather than threshold tuning.
9. The Android sender now includes a home-screen Quick connect widget that reloads saved receiver details and launches the mandatory MediaProjection consent flow. JVM tests, APK assembly, and lint pass; physical widget installation and launch remain pending.

Do not convert the failed legacy depth test into a claim that the new explicit-axis path works physically.

## Current implementation

### Quick panel and phone footer

- Landscape quick dashboard, 1024 × 600 RGBA texture.
- Show/Hide, Head, World, Left Dock, Right Dock, Settings, Quit.
- No primary-dashboard Glance, Pin, or Android Back.
- Phone footer: Android Back at far left; Move in the remaining central region; Close immediately left of far-right Resize. Close is no longer beside Back.
- Phone resize uses mouse X displacement, clamps width, and publishes settings on release. Landscape width is converted back to the base portrait convention before ordinary persistence.

### Show and Start Location

- Show resets a **world-space snapshot**, not a continuing head lock.
- Preserves the current submitted physical width, including controller scale when converting to world placement.
- Default Start Location: +0.35 m horizontal, 0 m vertical, 0.85 m forward in the captured HMD-local frame.
- Portable `StartLocation.h` computes the orientation so the panel normal faces the snapshot user. The OpenVR backend multiplies this once by the current HMD pose.
- The current reset uses configurable HMD-local coordinates; it does **not** query the dashboard's exact center or auto-adapt to dashboard size. User tuning is the supported way to accommodate setups.
- **Settings → Placement → Start Location** exposes Distance, Horizontal, Vertical sliders and `-`/`+` controls.
- Opening the section captures an HMD reference pose and shows a separate dummy phone box at current size/aspect. Updating sliders changes the preview transform, not the real phone position.
- Leaving the section or closing Settings hides the preview. Apply saves the profile, Cancel restores it; next dashboard Show uses the profile.
- Preview currently reuses the otherwise dormant gesture-progress overlay handle. It contains no streamed phone pixels. Before future gesture work, separate its surface/ownership from gesture progress so the two cannot overwrite each other.
- Settings store version **5** adds `start_distance`, `start_offset_x`, `start_offset_y`; versions 1–4 load with safe defaults. Validation rejects nonfinite/out-of-range values.

### Move and depth

- Resolve the originating mouse-event controller; if missing/non-controller, try the primary dashboard device, then a tracked hand with trigger pressed.
- Release ends an owned move without requiring the release event to repeat its device index.
- Tracking loss ends/fixes the panel at its last valid pose.
- Dashboard-close cancellation runs on visible→hidden transition, not on every dashboard-hidden frame.
- Legacy menu-button calibration polling is disabled in normal operation; Settings remains the supported calibration path.
- `/actions/phonecast_panel` binds only left/right stick position and is submitted at normal priority while the dashboard is visible. No panel axis set is submitted during dashboard-closed gameplay, and the shortcut set is no longer submitted while the dashboard owns focus.
- While a phone/settings Move handle is held, the invoking hand's explicit axis supplies depth: up farther, down closer along the head-to-panel ray.
- Portable `PanelDepth.h` supplies deadzone 0.20, maximum elapsed step 50 ms, maximum speed 0.6 m/s, range 0.20–3.0 m.
- Physical retesting confirmed that this explicit-action correction also fails to provide joystick depth while the dashboard owns focus. Preserve the bounded depth model, but research a supported dashboard-focused input route before another implementation attempt.

### Shortcuts

- With the dashboard closed: hold **either thumbstick button** to toggle phone visibility; double-click a stick to open/dock to that hand. If the visible phone is already docked to that same hand, repeating the double click snapshots its current pose and physical size into a stationary world anchor. The owner physically approved the double-click behavior.
- Long/double recognition is in SteamVR bindings, not application timers.
- Four optional Boolean actions in `/actions/phonecast_shortcuts`, normal priority `0`.
- Active+changed+pressed events dispatch existing portable commands. Dock takes precedence if docking and toggle are delivered together.
- Shortcut and panel-axis sets are mutually exclusive again after dashboard-active delivery physically failed. No broad overlay-global action set, trigger, grip, or calibration inputs are shipped.
- Both sets are remappable/unbindable through SteamVR. No in-app enable toggle has been added; defaults implement the owner's explicit request.
- Binding activity is logged, but do not treat manifest load as input-delivery proof. There is not yet a complete per-action diagnostics UI or action-orchestration regression fixture.
- A user-customized/cached SteamVR binding may retain old action paths after an update. If a new action is inactive, inspect/reset the PhoneCast binding rather than assuming the packaged defaults were adopted.

## Files to inspect first

- `docs/sprint-14-controls.md` — current behavior and physical matrix.
- `docs/framecorder-hotkey-research.md` — source-reviewed normal-priority shortcut reference.
- `docs/frametop-research.md` — interaction/keyboard/drag-safety references; not PhoneCast physical proof.
- `platform/openvr/src/OpenVrOverlayRenderer.cpp` — panel textures/hit targets, placement, preview, move/resize, input actions.
- `platform/openvr/input/phonecast-actions.json`.
- `platform/openvr/input/phonecast-bindings-frame-controller.json`.
- `vr/include/phonecast/vr/overlay/IOverlayRenderer.h` — view/settings contracts.
- `vr/include/phonecast/vr/interaction/StartLocation.h`.
- `vr/include/phonecast/vr/interaction/PanelDepth.h`.
- `vr/src/SettingsMenuController.cpp`, `vr/src/OverlaySettingsStore.cpp`.
- `apps/stream-receiver/src/vr_main.cpp` — command dispatch, stream-size normalization, persistence.
- `tests/phonecast_tests.cpp`, `tests/audio_tests.cpp`.

## Build/package and download state

- Windows: `out/build/windows-x64/bin/phonecast-vr-stream-receiver.exe`.
- ARM64 build: `out/build/linux-arm64-sprint14`.
- Published bundle: `out/packages/phonecast-steam-frame-arm64-sprint14.tar.gz`.
- Bundle SHA-256: `2ba52fbf130ae32d42ff090c2027f5b9a1d973fab5224b2d8785bb6f7332825e`.
- Packaged stripped receiver SHA-256: `e75f5616febc9b59c5aa89ac2c9287bd45aaffd1561ca51903554a735c00bcac`.
- Node server: **http://10.0.0.3:8080**.
- Routes: versioned `/phonecast-steam-frame-arm64-sprint14.tar.gz`, stable `/phonecast-steam-frame-arm64.tar.gz`, and `/install-phonecast.sh` bootstrap.
- Current observed Node PID: `101552`; ephemeral, recheck before managing it. Ignored `local-apk-server/server.pid` records it.
- Archive download over LAN was hash-verified. Page and bootstrap select Sprint 14.
- Artifact responses use `no-store` and read files per request. Replacing the archive needs no restart; changing server JS/page text needs a server restart.
- Package is produced inside Docker/Linux, stripped, executable bits preserved, complete installer/icons included. Package paths and output remain ignored.
- Quit PhoneCast before updating. Installer preserves settings/pairing. No Android changes/APK rebuild were required in Sprint 14.
- The agent did not SSH into/restart the headset or verify its installed receiver hash in this session. Do not assert an exact installed build from local publication alone.

## Software validation

Latest code:

- Windows build succeeded; all **12 CTest tests** passed.
- ARM64 cross-build succeeded.
- QEMU portable and audio tests passed.
- Both local download-server/bootstrap tests passed.
- Start Location tests cover user-facing normal, position retention, settings slider bounds, preview flag cleanup, transaction cancellation, version-5 round trip, version-4 defaults, and nonfinite input rejection.
- Existing depth tests cover sign, deadzone, bounds, not native action delivery.
- No new Android, SteamVR runtime, headset lifecycle, latency, or visual approval is implied.

## Recommended next work

1. Resume with the owner's next feedback rather than declaring Sprint 14 complete.
2. Confirm Start Location dummy preview, live edits, Apply/Cancel, persistence, unchanged size, facing direction, and rotation behavior.
3. Research why neither legacy polling nor the explicit axis action delivers Move-handle depth while the dashboard owns focus. Inspect `GetAnalogActionData` result/`bActive`, dashboard input ownership, and supported alternatives; do not increase global priority or activate axes during gameplay merely to force it.
4. Verify both-stick hold and per-hand double-click docking, dashboard-open suppression, transition-held buttons, tracking loss, and real-game conflicts in both launch orders.
5. Continue remaining Sprint 14 work: consistent lower controls across surfaces, keyboard/text entry, optional gesture recognizer, stronger input-release safety/automation and diagnostics. Avoid private Steam UI injection.
6. Update and hash-verify the serving site after each owner-testable native build; do not report source-only work as downloadable.

## Environment/safety reminders

- Windows CMake/CTest through PowerShell, WinLibs runtime prepended per `docs/development.md`.
- ARM64 cache records `/src`; use only Docker mounted at `/src` with `MSYS_NO_PATHCONV=1`, never Windows CMake. Debian packages include `cmake ninja-build g++-aarch64-linux-gnu git ca-certificates file libpulse-dev qemu-user`.
- QEMU: `qemu-aarch64 -L /usr/aarch64-linux-gnu ...`.
- Never start/restart SteamVR through Windows RDP.
- Do not restart/reconfigure headset audio services.
- If later read-only headset diagnostics are requested, use alias `frame` and ignored local credentials; never print/commit secrets.
- Do not mark physical behavior complete because builds or tests pass.
