# Sprint 14 control feedback iteration

## Current behavior

- The landscape quick panel no longer contains Android Back, Glance, or Pin.
- Dashboard Show resets the phone to a **standing/world-space anchor** to the right of the current quick panel. It samples the dashboard center and width using public OpenVR APIs, with an HMD-relative snapshot fallback if the dashboard transform is unavailable. It does not remain head-locked. Hide does not move the phone.
- The phone footer keeps Back at the far left, Close near the far right, and Resize at the rightmost corner. Move occupies the remaining footer area.
- Moving uses the actual pointer controller, with a primary-dashboard-controller fallback for missing mouse-event device indices. Mouse-up ends an owned drag even if its event does not repeat the originating tracked-device index.
- Holding the phone or settings Move handle and pushing the analog stick up moves the panel farther along the head-to-panel ray; down moves it closer. There is a 0.20 deadzone, bounded elapsed time, 0.6 m/s maximum speed, and 0.20–3.0 m distance clamp.
- Tracking loss freezes/releases a move at the last valid pose. Dashboard close releases active interactions on the visible-to-hidden transition, rather than repeatedly cancelling every frame for which the dashboard reports hidden.
- Selecting a dock with temporarily unavailable controller tracking retains the last visible transform and resumes attachment when tracking becomes valid, instead of hiding the phone after a failed settings application.
- Legacy menu-button calibration polling is disabled in ordinary use. Controller calibration remains available through Settings.
- Landscape renderer widths are converted back to the base portrait-size convention before persistence, avoiding repeated scale multiplication during rotation and placement updates.

## Left-controller shortcut

The shipped action manifest now contains **only** the optional Boolean toggle action in `/actions/phonecast_shortcuts`. The old broad trigger/grip/axis/calibration bindings are no longer shipped. The native Frame default binds a one-second **long press of the left thumbstick button**. No joystick axis, trigger, grip, or right-hand shortcut is bound.

The user explicitly requested this shortcut in this feedback iteration. It uses normal action-set priority `0`, following the source-reviewed Framecorder approach. It is user-remappable/unbindable through SteamVR bindings. Only active, changed presses toggle visibility. It does not cycle Glance states or reset placement. While the dashboard is visible, the shortcut action set is deactivated, and it must return to released state before rearming. Binding activity is logged without input contents or credentials. Initialization success is not evidence of delivery or conflict-free gameplay.

## Validation boundary

The owner physically reported that the previous resize corner worked, but movement and docking regressed and the Close button was too close to Back. That feedback does not approve this corrected build. The source fixes, coordinate/depth unit tests, Windows tests, and ARM64 compilation must be supplemented by physical validation of:

1. Show after first launch, world-stable beside-panel placement, and Hide/Show.
2. Phone and settings Move with both controllers, off-surface release, tracking loss, and dashboard closure.
3. Analog-stick farther/closer adjustment while holding Move, including clamps.
4. Left/right docking with tracking initially available and initially lost.
5. Back, Close, and Resize separation in portrait and landscape.
6. Left-stick long press with dashboard closed, pressed/released around dashboard transitions, and real-game control coexistence in both launch orders.

Software validation for this iteration: Windows build and all 12 CTest tests pass; Linux ARM64 cross-build and QEMU portable/audio tests pass; both download-server/bootstrap tests pass. The published archive was downloaded over `http://10.0.0.3:8080` and its hash matched the local file. Executable permissions were verified in the Linux-created archive. No SteamVR, receiver, game, or headset audio service was restarted; only the local download server was restarted.

Published bundle: `out/packages/phonecast-steam-frame-arm64-sprint14.tar.gz`  
SHA-256: `b47191758d9876e795cc60388734fa81ad065b7026d4e1fb7f440fcb87fe5b74`  
Packaged receiver SHA-256: `cb90a91b1f7faffbf80924635b3959bcd6c6aa9d42f5889704c553bbfcd64107`

Native renderer/decoder reliability follow-ups remain separate.
