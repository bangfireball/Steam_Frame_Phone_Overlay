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

## Phone-shaped control grid

A thumbstick long press or the wrist gesture opens a separate portrait OpenVR overlay without replacing the phone texture. The world-stable 2×5 grid contains Show/Hide, Glance, Pin, Settings, Head, World, Left, Right, and Close. Large cells accept OpenVR overlay laser/mouse clicks. The grid texture remains static while pointing to avoid repeated raw-texture submissions and the flicker observed during physical testing. Gaze selection is intentionally unsupported.

The long-press path retains its configurable 0.6-second default if SteamVR actions become available. The pose-only wrist path is the current fallback. Layout, scale, placement, and gesture threshold remain provisional pending physical review.

## In-headset settings

Choose **Settings** from the control grid to open a separate head-relative panel. It is transactional: changes preview live, **Apply** persists them, and **Cancel** or Back from the root restores the values from before the panel opened.

The portrait settings panel supports the same controller laser as the control grid. Click a row to open a category or activate an action. Adjustable rows show `-` and `+` targets at their left and right edges. Apply and Cancel remain explicit root actions. SteamVR Input navigation remains available as an optional fallback if button delivery becomes functional.

The panel contains Appearance, Placement, independent Left/Right Controller calibration, and Glance and Controls categories. Reset Position and Reset Hand are explicit row actions. Reset All requires a second confirmation click. Glance preview scale and radial-menu long-press duration are persisted with the overlay settings. `Ctrl+Alt+S` is a development fallback for opening the panel.

## Provisional wrist gesture and phone-shaped control grid

Because SteamVR/VRLink still did not deliver controller commands during physical testing, a controller-pose gesture opens the menu without button input:

1. begin with the controller face turned away from the headset so the gesture arms;
2. raise either controller within approximately 0.18–0.90 m of the headset;
3. turn its control face toward the headset and hold for 3 seconds;
4. a small circular indicator fills during the hold, and the world-stable portrait control panel opens only when it is full;
5. move the wrist freely and use the controller laser to hover and click a large grid cell.

There is deliberately no gaze selection. The panel remains open after the opening pose so the user does not have to maintain a difficult wrist angle while pointing. Its 2×5 grid contains Show/Hide, Glance, Pin, Settings, Head, World, Left, Right, and Close. It uses OpenVR overlay mouse events and normalized panel coordinates, matching the laser/UV interaction model planned for the phone screen.

The opening gesture uses tracked poses, not SteamVR button actions. The three-second dwell and segmented progress indicator respond to feedback that the original 0.45-second gesture activated too easily. Distance, vertical-position, facing-angle, indicator placement, and opening dwell remain provisional and require physical tuning. It must be checked for accidental activation during ordinary gameplay.

## Physical validation result and remaining work

A Steam Frame/VRLink test found that neither controller thumbstick click advanced the state machine, while the keyboard shortcuts worked. The receiver now uses explicit SteamVR Input actions instead of `IVRSystem::GetControllerState`, identifies its running process with the registered application key, and ships a `frame_controller` binding for both thumbsticks, both calibration buttons, axes, grips, and triggers. SteamVR accepted the action manifest and loaded the Frame binding, but a physical retest still delivered no controller commands and no radial menu on long press. The phone overlay itself was visible and stable and keyboard controls continued to work.

The retest established that loading bindings is insufficient for an overlay while another application owns controller focus. The receiver now requests OpenVR's overlay-global action-set priority. SteamVR documents this as experimental and requires **Settings > Developer > Experimental overlay input overrides** to be enabled. Physical validation of this priority path is pending; it must also check whether the bindings conflict unacceptably with game controls.

Keyboard-driven transitions and live, quick reveal were physically confirmed. Left/right keyboard placement selection now also selects the hand used by the next keyboard-driven Glance preview. This is not placement-mode cycling: only the Glance state temporarily attaches a 55%-scale copy of the live phone panel to the selected hand. The 55% preview size and offsets remain unapproved.

- Confirm whether the replacement action path receives thumbstick clicks from either controller.
- Physically validate the wrist-flip opening gesture, phone-shaped grid readability, laser hover/click selection, Close action, and false-positive rate.
- If button input becomes available, confirm short click still cycles and long press reliably opens the control grid without accidental activation.
- Review control-grid readability, placement, laser selection, cancellation, and the provisional opening dwell.
- Confirm the button does not conflict unacceptably with active games.
- Tune the 55% preview scale and controller offsets per hand.
- Verify all four transitions and immediate reveal with live video.
- Decide whether Expanded should later auto-dismiss and, if so, choose an inactivity interval from headset testing.
