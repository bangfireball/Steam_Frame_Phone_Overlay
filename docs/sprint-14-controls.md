# Sprint 14 control feedback iteration

## Owner-approved closure

Sprint 14 is complete for the owner-accepted baseline. The owner confirmed joystick
push/pull works, accepted consistent panel controls, move/resize safety, the Android
widget, and lifecycle behavior, and requested commit/push. Keyboard/text entry and
wrist gestures are backlogged as unnecessary for current use. Further lifecycle
checks proceed organically, without claiming every original matrix case was
separately exercised.

All hotkeys were available during gameplay. This confirms delivery, not absence
of conflicts. Per-hotkey enable/disable toggles are backlogged to mitigate that
risk; none is implemented in this closure. SteamVR remapping/unbinding remains
available. Dashboard-visible arbitrary hotkeys still do not work: depth uses
supported overlay scroll events, not SteamVR Input action delivery.

## Current behavior

- The landscape quick panel no longer contains Android Back, Glance, or Pin.
- Dashboard Show resets the phone to a **standing/world-space anchor** using the user's Start Location profile. The safer default is +0.35 m horizontal, 0 m vertical, and 0.85 m forward in a captured HMD-local frame. Its orientation faces the user at that snapshot, rather than copying a parallel panel orientation. It does not remain head-locked. Show preserves the last physical width; Hide does not move the phone.
- Open **Settings → Placement → Start Location** to adjust Distance, Horizontal, and Vertical with sliders or `-`/`+`. A separate dummy phone box appears on entry at the current phone size/aspect and moves live as values change. Its captured reference pose remains stable while editing. It contains no streamed phone pixels. Leaving the section or closing Settings removes it. Apply persists the profile; Cancel restores the original profile. Editing this profile does not move the real phone until the next dashboard Show. Version 5 settings migrate versions 1–4 with the safer new defaults.
- The phone footer keeps Back at the far left, Close near the far right, and Resize at the rightmost corner. Move occupies the remaining footer area.
- Moving uses the actual pointer controller, with a primary-dashboard-controller fallback for missing mouse-event device indices. Mouse-up ends an owned drag even if its event does not repeat the originating tracked-device index.
- Holding the phone or Settings Move handle now routes the owning controller's supported overlay **smooth scroll** events into head-to-panel-ray depth adjustment. Positive delta means farther, negative nearer (push/pull physically confirmed by the owner). Smooth deltas are increments, not axes: no deadzone or elapsed-time integration. Each unit maps to 0.05 m, capped to ±0.03 m per event-pump pass and 0.20–3.0 m distance. Discrete events are counted but not applied, avoiding double movement when the runtime emits both forms. Scroll during phone movement is never forwarded to Android, including events from the other hand. Both panels request scroll delivery. Release, dashboard closure, focus leave, overlay hiding, and tracking loss clear pending depth. Per-drag `openvr-depth` summaries count smooth/discrete and positive/negative events without pointer/content logging. The owner physically confirmed that the fix allows pushing/pulling the window.
- Tracking loss freezes/releases a move at the last valid pose. Dashboard close releases active interactions on the visible-to-hidden transition, rather than repeatedly cancelling every frame for which the dashboard reports hidden.
- Selecting a dock with temporarily unavailable controller tracking retains the last visible transform and resumes attachment when tracking becomes valid, instead of hiding the phone after a failed settings application.
- Legacy menu-button calibration polling is disabled in ordinary use. Controller calibration remains available through Settings.
- Landscape renderer widths are converted back to the base portrait-size convention before persistence, avoiding repeated scale multiplication during rotation and placement updates.

## Thumbstick shortcuts

The shipped action manifest now has only the narrow shortcut action set. `/actions/phonecast_shortcuts` is submitted only while the dashboard is closed. A binding-layer long press on **either** thumbstick button toggles visibility without resetting placement. A binding-layer double click opens the phone and docks it to the controller whose stick was clicked. Repeating the double click while the visible phone is already docked to that same controller converts its current tracked pose and physical size into a stationary world anchor. While the dashboard is visible, no action set is submitted. The failed `/actions/phonecast_panel` axis set and bindings have been removed. The old trigger/grip/calibration bindings are not shipped.

The user physically confirmed that the normal-priority shortcuts do not deliver while the SteamVR dashboard owns focus, even when PhoneCast submitted their action set alongside the former panel set. Both dashboard-active action experiments have been removed. The dashboard laser remains the interaction route after summoning the phone. The dashboard-closed shortcut path acts directly on active changed events rather than requiring a separately observed released sample first. The shortcut set uses normal action-set priority `0`, following the source-reviewed Framecorder approach, and are user-remappable/unbindable through SteamVR. Binding activity is logged without input contents or credentials.

## Validation boundary

Latest owner feedback physically approves repeated double-click docking/world pinning, the Start Location settings and preview, and the phone footer's Back, Close, and Resize controls and appearance. Joystick push/pull is now physically confirmed. The owner accepted controls, safety, widget, and lifecycle behavior for closure. The following earlier detailed matrix is retained for organic follow-up, not a sprint blocker or a claim that each individual case was exercised:

1. Start Location persistence and lifecycle edge cases beyond the approved normal settings/preview flow.
2. Phone and settings Move with both controllers, off-surface release, tracking loss, and dashboard closure.
3. Test the supported overlay-scroll-event depth route proposed in [`sprint-14-depth-input-research.md`](sprint-14-depth-input-research.md), then validate both controllers and clamps.
4. Left/right docking with tracking initially unavailable or lost.
5. Footer controls across repeated portrait/landscape transitions and lifecycle recovery beyond the approved normal use.
6. Either-stick hold reliability, dashboard suppression/transitions, and real-game control coexistence in both launch orders.

Software validation for this iteration: Windows build and all 12 CTest tests pass; Linux ARM64 cross-build and QEMU portable/audio tests pass; both download-server/bootstrap tests pass. The published archive was downloaded over `http://10.0.0.3:8080` and its hash matched the local file. Executable permissions were verified in the Linux-created archive. No SteamVR, receiver, game, or headset audio service was restarted; only the local download server was restarted.

Published bundle: `out/packages/phonecast-steam-frame-arm64-sprint14.tar.gz`

SHA-256: `beba327e4d66ca63ad2aec61ea5ef8b74dddc7503bfc2937efcbf358b509d1a5`

Packaged receiver SHA-256: `873ddb28ba771cefa29db55d4da18dc8326a47c6f41ab141de1f36b1822ce716`

Native renderer/decoder reliability follow-ups remain separate.
