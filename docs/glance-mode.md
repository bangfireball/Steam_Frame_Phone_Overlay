# Sprint 6 — Glance mode

## State model

Glance mode is portable receiver state and does not depend on OpenVR:

```text
Hidden -> Glance -> Expanded -> Pinned -> Hidden
```

- **Hidden**: the compositor overlay is hidden, but decoding continues so reveal is immediate.
- **Glance**: a 55% preview is attached to the controller that requested it.
- **Expanded**: the user's normal persisted placement and size are restored.
- **Pinned**: the normal view remains visible until explicitly cycled or dismissed. This distinct state allows future transient-dismiss behavior without changing the input flow.

Glance state is intentionally session-only. Normal placement, appearance, and independent hand calibration remain persisted by `OverlaySettingsStore`; entering Glance does not overwrite them.

## Input

The button fallback is implemented first because it is deterministic and can be tested without guessing a universal wrist pose:

- Click either controller's primary axis/touchpad outside calibration mode to advance the state machine. The initiating hand owns the Glance preview.
- `Ctrl+Alt+G` advances the same state machine using the last selected hand.
- `Ctrl+Alt+P` toggles directly between Hidden and Expanded.

Controller input is translated by the OpenVR backend into a portable `GlanceInput`; OpenVR button identifiers do not enter the state model.

## Experimental radial menu

A thumbstick long press provides a controller-only command surface without delaying the normal short-click action:

1. hold either thumbstick click for 0.6 seconds;
2. release after the radial menu appears on that hand;
3. move the thumbstick to highlight an item;
4. click to confirm, or press left View/right Menu to cancel.

The eight items are Show/Hide, Glance, Pin, Right, Settings, World, Head, and Left. Placement selections reveal a pinned full-size panel. The menu is a separate OpenVR overlay and does not replace the phone texture. The 0.6-second default threshold, layout, scale, controller pose, and selection behavior are intentionally provisional pending physical review and may change.

## In-headset settings

Choose **Settings** from the radial menu to open a separate head-relative panel. It is transactional: changes preview live, **Apply** persists them, and **Cancel** or Back from the root restores the values from before the panel opened.

Controller navigation uses the hand that opened the radial menu:

- thumbstick up/down selects a row;
- thumbstick left/right changes the selected value;
- thumbstick click opens a category or activates an action;
- left View or right Menu goes back; from the root it cancels and closes settings.

The panel contains Appearance, Placement, independent Left/Right Controller calibration, and Glance and Controls categories. Reset Position and Reset Hand are explicit row actions. Reset All requires a second confirmation click. Glance preview scale and radial-menu long-press duration are persisted with the overlay settings. `Ctrl+Alt+S` is a development fallback for opening the panel.

## Wrist-gesture investigation

A reliable automatic wrist reveal needs more than a controller's instantaneous orientation. Controller models and grip poses differ, users rotate their forearms during ordinary play, tracking can be temporarily invalid, and a simple palm-up threshold would repeatedly reveal the panel during gameplay. A production gesture should include:

1. per-hand calibrated palm/up axes rather than a fixed controller-space axis;
2. HMD-relative visibility and distance checks;
3. angular threshold hysteresis;
4. a dwell interval before reveal;
5. a cooldown after dismissal;
6. suppression while grabbing, calibrating, or interacting with a game.

Automatic gesture detection is therefore deferred until the button flow and Sprint 5.1 hand poses are physically reviewed. The fallback satisfies the same reveal flow and provides a baseline against which gesture false positives can be judged.

## Physical validation result and remaining work

A Steam Frame/VRLink test found that neither controller thumbstick click advanced the state machine, while the keyboard shortcuts worked. The receiver now uses explicit SteamVR Input actions instead of `IVRSystem::GetControllerState`, identifies its running process with the registered application key, and ships a `frame_controller` binding for both thumbsticks, both calibration buttons, axes, grips, and triggers. SteamVR accepted the action manifest and loaded the Frame binding; physical input delivery still needs retesting.

Keyboard-driven transitions and live, quick reveal were physically confirmed. Left/right keyboard placement selection now also selects the hand used by the next keyboard-driven Glance preview. This is not placement-mode cycling: only the Glance state temporarily attaches a 55%-scale copy of the live phone panel to the selected hand. The 55% preview size and offsets remain unapproved.

- Confirm the replacement action path receives thumbstick clicks from both controllers.
- Confirm short click still cycles and long press reliably opens the radial menu without accidental activation.
- Review radial-menu readability, pose, selection, cancellation, and the provisional 0.6-second threshold.
- Confirm the button does not conflict unacceptably with active games.
- Tune the 55% preview scale and controller offsets per hand.
- Verify all four transitions and immediate reveal with live video.
- Decide whether Expanded should later auto-dismiss and, if so, choose an inactivity interval from headset testing.
