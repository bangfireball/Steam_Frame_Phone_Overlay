# Sprint 14 control feedback iteration

## Current behavior

- The landscape quick panel no longer contains Android Back, Glance, or Pin.
- Dashboard Show resets the phone to a **standing/world-space anchor** using the user's Start Location profile. The safer default is +0.35 m horizontal, 0 m vertical, and 0.85 m forward in a captured HMD-local frame. Its orientation faces the user at that snapshot, rather than copying a parallel panel orientation. It does not remain head-locked. Show preserves the last physical width; Hide does not move the phone.
- Open **Settings → Placement → Start Location** to adjust Distance, Horizontal, and Vertical with sliders or `-`/`+`. A separate dummy phone box appears on entry at the current phone size/aspect and moves live as values change. Its captured reference pose remains stable while editing. It contains no streamed phone pixels. Leaving the section or closing Settings removes it. Apply persists the profile; Cancel restores the original profile. Editing this profile does not move the real phone until the next dashboard Show. Version 5 settings migrate versions 1–4 with the safer new defaults.
- The phone footer keeps Back at the far left, Close near the far right, and Resize at the rightmost corner. Move occupies the remaining footer area.
- Moving uses the actual pointer controller, with a primary-dashboard-controller fallback for missing mouse-event device indices. Mouse-up ends an owned drag even if its event does not repeat the originating tracked-device index.
- Holding the phone or settings Move handle activates a dashboard-only panel action set intended to move the panel along the head-to-panel ray with the invoking hand's explicit SteamVR Input axis. Physical retesting found that this explicit axis still does not deliver usable depth adjustment while the dashboard owns focus. The implementation retains a 0.20 deadzone, bounded elapsed time, 0.6 m/s maximum speed, and 0.20–3.0 m distance clamp, but the input route requires further research rather than threshold tuning.
- Tracking loss freezes/releases a move at the last valid pose. Dashboard close releases active interactions on the visible-to-hidden transition, rather than repeatedly cancelling every frame for which the dashboard reports hidden.
- Selecting a dock with temporarily unavailable controller tracking retains the last visible transform and resumes attachment when tracking becomes valid, instead of hiding the phone after a failed settings application.
- Legacy menu-button calibration polling is disabled in ordinary use. Controller calibration remains available through Settings.
- Landscape renderer widths are converted back to the base portrait-size convention before persistence, avoiding repeated scale multiplication during rotation and placement updates.

## Thumbstick shortcuts

The shipped action manifest has two mutually exclusive narrow action sets. `/actions/phonecast_shortcuts` is submitted only while the dashboard is closed. A binding-layer long press on **either** thumbstick button toggles visibility without resetting placement. A binding-layer double click opens the phone and docks it to the controller whose stick was clicked. Repeating the double click while the visible phone is already docked to that same controller converts its current tracked pose and physical size into a stationary world anchor. While the dashboard is visible, only `/actions/phonecast_panel` is submitted; it binds left/right stick position for move-handle depth adjustment. The old trigger/grip/calibration bindings are not shipped.

The user physically confirmed that the normal-priority shortcuts do not deliver while the SteamVR dashboard owns focus, even when PhoneCast submits their action set alongside the panel set. That experiment has been removed. The dashboard laser remains the interaction route after summoning the phone. The dashboard-closed shortcut path acts directly on active changed events rather than requiring a separately observed released sample first. Both sets use normal action-set priority `0`, following the source-reviewed Framecorder approach, and are user-remappable/unbindable through SteamVR. Binding activity is logged without input contents or credentials.

## Validation boundary

Latest owner feedback physically approves repeated double-click docking/world pinning, the Start Location settings and preview, and the phone footer's Back, Close, and Resize controls and appearance. Joystick depth adjustment still does not work while the dashboard owns focus. Remaining physical validation includes:

1. Start Location persistence and lifecycle edge cases beyond the approved normal settings/preview flow.
2. Phone and settings Move with both controllers, off-surface release, tracking loss, and dashboard closure.
3. Test the supported overlay-scroll-event depth route proposed in [`sprint-14-depth-input-research.md`](sprint-14-depth-input-research.md), then validate both controllers and clamps.
4. Left/right docking with tracking initially unavailable or lost.
5. Footer controls across repeated portrait/landscape transitions and lifecycle recovery beyond the approved normal use.
6. Either-stick hold reliability, dashboard suppression/transitions, and real-game control coexistence in both launch orders.

Software validation for this iteration: Windows build and all 12 CTest tests pass; Linux ARM64 cross-build and QEMU portable/audio tests pass; both download-server/bootstrap tests pass. The published archive was downloaded over `http://10.0.0.3:8080` and its hash matched the local file. Executable permissions were verified in the Linux-created archive. No SteamVR, receiver, game, or headset audio service was restarted; only the local download server was restarted.

Published bundle: `out/packages/phonecast-steam-frame-arm64-sprint14.tar.gz`

SHA-256: `2ba52fbf130ae32d42ff090c2027f5b9a1d973fab5224b2d8785bb6f7332825e`

Packaged receiver SHA-256: `e75f5616febc9b59c5aa89ac2c9287bd45aaffd1561ca51903554a735c00bcac`

Native renderer/decoder reliability follow-ups remain separate.
