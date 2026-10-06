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

### Not yet tested

- Whether scroll events continue while trigger is held on the Move handle.
- Whether Steam Frame emits smooth, discrete, or both event forms for the relevant stick gesture.
- Event sign, cadence, hand identity, and behavior on the Settings overlay.

## Primary references

- Valve OpenVR header: https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h
- Valve SteamVR Input guide: https://github.com/ValveSoftware/openvr/wiki/SteamVR-Input
- Valve overlay overview: https://github.com/ValveSoftware/openvr/wiki/IVROverlay_Overview
- Existing PhoneCast dashboard-focus research: [`frame-passthrough-shortcuts-research.md`](frame-passthrough-shortcuts-research.md)
