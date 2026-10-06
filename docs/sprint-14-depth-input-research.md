# Sprint 14 dashboard joystick-depth research

Research date: 2026-10-07

## Result

PhoneCast should stop trying to read a private thumbstick axis while the SteamVR dashboard owns input focus. Both attempted routes have now failed physically on Steam Frame:

1. legacy controller-state polling; and
2. a normal-priority SteamVR Input `vector2` action submitted while the dashboard is visible.

This matches the existing dashboard-focus evidence for Boolean hotkeys: even overlay-global public action priority does not override dashboard focus. Raising action priority or globally claiming the joystick would add game-input risk without evidence that it can solve the dashboard case.

The best supported next experiment is to use the dashboard laser's existing **overlay scroll events**, not a separate action set. OpenVR defines:

- `VROverlayFlags_SendVRDiscreteScrollEvents`, which sends mouse-wheel-like `VREvent_ScrollDiscrete` events and requires mouse input mode;
- `VROverlayFlags_SendVRSmoothScrollEvents`, which sends trackpad-style `VREvent_ScrollSmooth` events and requires mouse input mode; and
- `VREvent_Scroll_t`, whose `xdelta`/`ydelta` and enclosing tracked-device index can be routed through the current drag owner.

These are part of the same overlay event stream that already delivers dashboard laser movement and trigger presses. PhoneCast already enables both scroll flags on the phone overlay and physically validated runtime scrolling for Android interaction. Its current event handler deliberately suppresses scroll forwarding while the pointer is over the Move handle, but does not yet reinterpret those events as depth. The Settings overlay uses mouse input but does not currently request scroll events.

## Proposed bounded experiment

1. Add diagnostics only first: while a phone or Settings Move drag is owned, record aggregate event type, count, sign, and non-sensitive delta range for discrete/smooth scroll events. Do not log pointer coordinates or phone content.
2. Confirm on Steam Frame whether pushing the invoking controller's stick while holding Move produces scroll events and whether up/down signs are stable.
3. If confirmed, route only the owned drag's scroll events into the existing portable `AdjustPanelDepth` bounds. Do not submit `/actions/phonecast_panel` in this path.
4. Request the same scroll flag on the Settings overlay.
5. Select either smooth or discrete delivery after observation; do not apply both if the runtime emits duplicates.
6. Release ownership on trigger-up, dashboard close, tracking loss, focus loss, or overlay destruction. Ignore scroll when no Move drag is active so normal phone scrolling remains unchanged.
7. Physically validate both hands, phone and Settings panels, near/far clamps, dashboard transitions, portrait/landscape, and no accidental Android scroll during movement.

If the runtime does not emit scroll events during a Move drag, the next supported fallback is explicit on-panel nearer/farther buttons or a laser-controlled depth slider. Undocumented `vrserver` input feeds and permanent overlay-global joystick claims are rejected.

## Implemented experiment — 2026-10-07

The owner requested an implementation attempt. Phone and Settings Move drags now
consume only the owning controller's smooth overlay scroll events; discrete
scroll is counted, not applied, so duplicate forms cannot move twice. Positive
smooth delta maps to farther along the current head-to-panel ray, negative nearer.
A unit maps to 0.05 m, with accumulated displacement capped to ±0.03 m per pump
and distance clamped to 0.20–3.0 m. There is no polled-axis deadzone/time scaling.
The failed panel action set and its joystick bindings have been removed; dashboard
operation submits zero active action sets. Ordinary phone scrolling is retained
outside owned Move/Resize interactions.

Drag release logs aggregate smooth/discrete counts and delta sign counts under
`openvr-depth`. Focus leave, overlay hiding, dashboard closure, and tracking loss
release ownership and clear pending deltas. Missing event device indices use the
existing primary-dashboard-controller fallback; identifiable other-hand events
cannot adjust depth. Settings requests both scroll flags as well.

Windows build/all 12 CTest tests pass. ARM64 cross-build, QEMU portable/audio/runtime
tests, and both download-server tests pass. The initial container CTest invocation
lacked the ARM64 loader prefix; rerunning with `QEMU_LD_PREFIX=/usr/aarch64-linux-gnu`
passed. Tests cover sign, tiny deltas, duplicate discrete suppression, wrong-owner
rejection, nonfinite input, burst caps, distance clamps, consumption, and reset.
These do not prove runtime scroll delivery during a held-trigger drag.

Physical retest: quit PhoneCast, update the Frame bundle, reopen the dashboard,
and hold phone Move while pushing the same hand's stick up/down. Repeat with
Settings and each hand; check direction, speed, clamps, off-surface release,
dashboard closure, tracking loss, rotation, and no Android scroll during movement.
If nothing moves, inspect the released drag's `openvr-depth` counts: zero smooth
with nonzero discrete suggests a discrete-only runtime; both zero means this
runtime does not deliver the candidate route during drag. Do not enable both
forms blindly or restore global axis claims. No hardware/runtime was restarted
by the agent and physical approval remains pending.

Published archive: `out/packages/phonecast-steam-frame-arm64-sprint14.tar.gz`.
The live stable download at `http://10.0.0.3:8080/phonecast-steam-frame-arm64.tar.gz`
was fetched and its SHA-256 matched the local archive; no server restart was needed.

- Archive SHA-256: `beba327e4d66ca63ad2aec61ea5ef8b74dddc7503bfc2937efcbf358b509d1a5`
- Receiver SHA-256: `873ddb28ba771cefa29db55d4da18dc8326a47c6f41ab141de1f36b1822ce716`

## Physical result and closure

The owner physically confirmed: “the joystick fix worked. i am now able to push
and pull the window.” This validates the supported overlay-scroll depth route.
It does not establish general SteamVR Input hotkey delivery while the dashboard
is visible. The prior read-only log snapshot showed the previous installed hash;
that snapshot predates this approval and cannot identify the build used for the
successful physical test. Exact per-hand/edge-case results were not separately
reported. The owner later accepted move/resize safety and closed Sprint 14.

## Evidence boundaries

### Documented

- OpenVR 2.15.6 header comments define discrete and smooth overlay scroll flags as mouse-input-mode event sources.
- `IsActiveDashboardOverlay` can distinguish the selected dashboard tab.
- `GetPrimaryDashboardDevice` identifies the dashboard laser controller or most recently used device.

### Physically tested

- Dashboard laser mouse/trigger events reach PhoneCast.
- Runtime scroll reaches the phone interaction path.
- Legacy controller-state depth input did not work while the dashboard was active.
- Normal-priority explicit axis depth input did not work while the dashboard was active.
- Normal-priority Boolean hotkeys also did not deliver while the dashboard owned focus.

### Remaining detailed evidence

- Push/pull during an owned Move drag is physically confirmed; exact smooth/discrete event counts were not collected from the successful test.
- Per-hand, Settings-specific, sign/cadence, and edge-case measurements were not individually reported; the owner accepted the move/resize baseline without requiring a separately recorded result for every case.

## Primary references

- Valve OpenVR header: https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h
- Valve SteamVR Input guide: https://github.com/ValveSoftware/openvr/wiki/SteamVR-Input
- Valve overlay overview: https://github.com/ValveSoftware/openvr/wiki/IVROverlay_Overview
- Existing PhoneCast dashboard-focus research: [`frame-passthrough-shortcuts-research.md`](frame-passthrough-shortcuts-research.md)
