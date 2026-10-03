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

## Wrist-gesture investigation

A reliable automatic wrist reveal needs more than a controller's instantaneous orientation. Controller models and grip poses differ, users rotate their forearms during ordinary play, tracking can be temporarily invalid, and a simple palm-up threshold would repeatedly reveal the panel during gameplay. A production gesture should include:

1. per-hand calibrated palm/up axes rather than a fixed controller-space axis;
2. HMD-relative visibility and distance checks;
3. angular threshold hysteresis;
4. a dwell interval before reveal;
5. a cooldown after dismissal;
6. suppression while grabbing, calibrating, or interacting with a game.

Automatic gesture detection is therefore deferred until the button flow and Sprint 5.1 hand poses are physically reviewed. The fallback satisfies the same reveal flow and provides a baseline against which gesture false positives can be judged.

## Physical validation still required

- Confirm the runtime exposes primary-axis/touchpad clicks on both Steam Frame/VRLink controllers.
- Confirm the button does not conflict unacceptably with active games.
- Tune the 55% preview scale and controller offsets per hand.
- Verify all four transitions and immediate reveal with live video.
- Decide whether Expanded should later auto-dismiss and, if so, choose an inactivity interval from headset testing.
