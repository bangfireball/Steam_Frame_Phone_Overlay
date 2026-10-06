# Sprint 14 control feedback iteration

## Current behavior

- The landscape quick panel no longer contains Android Back, Glance, or Pin.
- Dashboard Show resets the phone to a **standing/world-space anchor** using the user's Start Location profile. The safer default is +0.35 m horizontal, 0 m vertical, and 0.85 m forward in a captured HMD-local frame. Its orientation faces the user at that snapshot, rather than copying a parallel panel orientation. It does not remain head-locked. Show preserves the last physical width; Hide does not move the phone.
- Open **Settings → Placement → Start Location** to adjust Distance, Horizontal, and Vertical with sliders or `-`/`+`. A separate dummy phone box appears on entry at the current phone size/aspect and moves live as values change. Its captured reference pose remains stable while editing. It contains no streamed phone pixels. Leaving the section or closing Settings removes it. Apply persists the profile; Cancel restores the original profile. Editing this profile does not move the real phone until the next dashboard Show. Version 5 settings migrate versions 1–4 with the safer new defaults.
- The phone footer keeps Back at the far left, Close near the far right, and Resize at the rightmost corner. Move occupies the remaining footer area.
- Moving uses the actual pointer controller, with a primary-dashboard-controller fallback for missing mouse-event device indices. Mouse-up ends an owned drag even if its event does not repeat the originating tracked-device index.
- Holding the phone or settings Move handle activates a dashboard-only panel action set. The invoking hand's explicit SteamVR Input axis moves the panel along the head-to-panel ray: up is farther and down is closer. This replaces the legacy `GetControllerState` axis read that did not deliver movement in the physical retest. There is a 0.20 deadzone, bounded elapsed time, 0.6 m/s maximum speed, and 0.20–3.0 m distance clamp.
- Tracking loss freezes/releases a move at the last valid pose. Dashboard close releases active interactions on the visible-to-hidden transition, rather than repeatedly cancelling every frame for which the dashboard reports hidden.
- Selecting a dock with temporarily unavailable controller tracking retains the last visible transform and resumes attachment when tracking becomes valid, instead of hiding the phone after a failed settings application.
- Legacy menu-button calibration polling is disabled in ordinary use. Controller calibration remains available through Settings.
- Landscape renderer widths are converted back to the base portrait-size convention before persistence, avoiding repeated scale multiplication during rotation and placement updates.

## Thumbstick shortcuts

The shipped action manifest has two narrow action sets. `/actions/phonecast_shortcuts` is submitted whether the dashboard is open or closed. A binding-layer long press on **either** thumbstick button toggles visibility without resetting placement. A binding-layer double click opens the phone and docks it to the controller whose stick was clicked. While the dashboard is visible, `/actions/phonecast_panel` is submitted alongside it and binds only left/right stick position for move-handle depth adjustment. The old trigger/grip/calibration bindings are not shipped.

The user explicitly requested dashboard-active shortcuts as a physical experiment after confirming that dashboard-closed left-stick hold worked but sometimes required multiple attempts. The revised path acts directly on active changed events rather than requiring a separately observed released sample first. Both sets use normal action-set priority `0`, following the source-reviewed Framecorder approach, and are user-remappable/unbindable through SteamVR. Binding activity is logged without input contents or credentials. OpenVR may suppress or arbitrate same-source bindings while the dashboard owns focus, so compilation and manifest activation are not proof that dashboard-active delivery works or that dashboard navigation remains conflict-free.

## Validation boundary

The owner physically reported that the previous resize corner worked, but movement and docking regressed and the Close button was too close to Back. That feedback does not approve this corrected build. The source fixes, coordinate/depth unit tests, Windows tests, and ARM64 compilation must be supplemented by physical validation of:

1. Show after first launch, unchanged size, closer placement facing the user, and world-stable Hide/Show; Start Location dummy preview, live sliders, Apply/Cancel, persistence, and preview cleanup.
2. Phone and settings Move with both controllers, off-surface release, tracking loss, and dashboard closure.
3. Explicit-action analog-stick farther/closer adjustment while holding Move with either controller, including clamps.
4. Left/right docking with tracking initially available and initially lost.
5. Back, Close, and Resize separation in portrait and landscape.
6. Either-stick long press toggling and either-stick double-click docking with the dashboard open and closed, including dashboard transitions, dashboard navigation coexistence, and real-game control coexistence in both launch orders.

Software validation for this iteration: Windows build and all 12 CTest tests pass; Linux ARM64 cross-build and QEMU portable/audio tests pass; both download-server/bootstrap tests pass. The published archive was downloaded over `http://10.0.0.3:8080` and its hash matched the local file. Executable permissions were verified in the Linux-created archive. No SteamVR, receiver, game, or headset audio service was restarted; only the local download server was restarted.

Published bundle: `out/packages/phonecast-steam-frame-arm64-sprint14.tar.gz`

SHA-256: `22e36bce1a3034fd8a897d820a8653f9a340fe2d1250f6b764b2f1027458684a`

Packaged receiver SHA-256: `5bbe08e66cc45e5c9da7c64ec185f4cdfaaf15a7214d44c99c01d78a71837f55`

Native renderer/decoder reliability follow-ups remain separate.
