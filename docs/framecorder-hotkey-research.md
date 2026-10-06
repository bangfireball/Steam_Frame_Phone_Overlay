# Framecorder dashboard-closed hotkey research

Research date: 2026-10-06  
Source reviewed: [`coah80/framecorder`](https://github.com/coah80/framecorder) at commit [`171a905006bb917e5a1677fa922a1d00cc1ec214`](https://github.com/coah80/framecorder/tree/171a905006bb917e5a1677fa922a1d00cc1ec214)  
Scope: the native Steam Frame clip shortcut and implications for PhoneCast Sprint 14

## Result

Framecorder implements its dashboard-closed clip shortcut with a very small SteamVR Input surface:

- one `leftright` action set, `/actions/framecorder`;
- one optional Boolean input action, `/actions/framecorder/in/clip`;
- one optional haptic output;
- a Frame Controller binding that maps only the **left thumbstick button's `long` mode** to the clip action;
- normal action-set priority `0`, not OpenVR's overlay-global priority range;
- the standard `UpdateActionState` / `GetDigitalActionData` flow;
- firing only when the action is active, changed, and pressed.

The binding layer owns the long-press timing. Framecorder does not continuously claim the trigger, grip, thumbstick axes, or both controllers. Its source comment states that overlay applications receive their bindings alongside the game's bindings. The project owner's physical result confirms that Framecorder's shortcut is available with the dashboard closed on the target headset; that result is evidence for Framecorder, not yet for PhoneCast.

Framecorder writes its embedded action manifest and binding to the user's XDG data directory at startup, then passes the resulting absolute manifest path to SteamVR. It also exposes a haptic action so a successful clip can be acknowledged on the hand to which the action is bound.

## Comparison with the earlier passthrough-shortcuts reference

The previously reviewed Frame Passthrough Shortcuts project uses maximum overlay-global priority and explicitly reports that dashboard focus blocks its shortcuts. Framecorder instead submits a normal-priority action set containing one narrow action. This means PhoneCast should not assume overlay-global priority is required on native Steam Frame.

The projects also agree on the safer aspects of a future PhoneCast experiment:

- use SteamVR's binding modes for long/double presses;
- keep the action set narrow and user-remappable;
- inspect per-action `bActive`, `bState`, and `bChanged` rather than treating manifest loading as proof;
- do not expect a background shortcut to replace dashboard controls;
- test dashboard-open and dashboard-closed behavior independently.

## PhoneCast decision

Do **not** reactivate PhoneCast's dormant broad action set. Its trigger, grip, axes, menu buttons, and both thumbstick inputs are too invasive for ordinary gameplay.

When Sprint 14 reaches the hotkey experiment, create a separate opt-in action set containing only approved Boolean commands. Start with one Frame Controller thumbstick-button `long` or `double` binding, normal priority `0`, and per-action activity diagnostics. Add haptic confirmation only after action delivery and game-input coexistence are physically established. Overlay-global priority may remain a controlled comparison variable, not the default.

The dashboard remains the recovery path when a shortcut is unbound, inactive, disabled, or unavailable.

## Evidence boundaries

### Source-reviewed

- action and action-set declarations in `assets/input/actions.json`;
- left-thumbstick long-press binding in `assets/input/bindings_frame_controller.json`;
- manifest installation, handle lookup, priority `0`, action polling, and haptic dispatch in `src/input.rs`.

### User-reported physical evidence

- Framecorder's hotkey works on Steam Frame while its dashboard is closed.

### Not yet tested by PhoneCast

- whether a normal-priority narrow PhoneCast action is active over standalone VR scenes;
- whether the same binding coexists with controls in multiple games;
- dashboard-open suppression and release behavior;
- rebinding, unbinding, one-controller loss, sleep/wake, and SteamVR restart;
- Windows/VRLink behavior.

## Sources

- Repository: https://github.com/coah80/framecorder
- Input implementation: https://github.com/coah80/framecorder/blob/171a905006bb917e5a1677fa922a1d00cc1ec214/src/input.rs
- Action manifest: https://github.com/coah80/framecorder/blob/171a905006bb917e5a1677fa922a1d00cc1ec214/assets/input/actions.json
- Frame Controller binding: https://github.com/coah80/framecorder/blob/171a905006bb917e5a1677fa922a1d00cc1ec214/assets/input/bindings_frame_controller.json
