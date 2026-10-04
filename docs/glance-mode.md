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

## SteamVR dashboard launcher

The OpenVR backend now creates a best-effort dashboard overlay alongside the regular phone overlays. When the receiver starts successfully, `CreateDashboardOverlay` supplies a PhoneCast dashboard panel and thumbnail/icon. The panel provides large laser-selectable Show/Hide, Glance, Pin, Settings, Head, World, Left, and Right controls and routes them through the same portable actions as the phone-shaped control grid.

Dashboard creation is deliberately non-fatal: if a runtime does not expose the tab, normal phone streaming and keyboard controls continue. The icon, panel, and controller-laser interaction are physically approved on the Windows/VRLink path and are now the supported in-headset entry point. Native ARM64 lifecycle remains unvalidated. Native Quick Access injection is not part of this supported OpenVR path, and autostart has not been enabled.

## Deferred phone-shaped control grid

The earlier thumbstick long-press and wrist-pose paths can open a separate portrait OpenVR control overlay without replacing the phone texture. These experimental entry paths are now disabled at runtime because the dashboard provides a reliable supported control surface. Their code is retained only as dormant research and must not be presented as required or active behavior.

## In-headset settings

Choose **Settings** from the control grid to open a separate head-relative panel. It is transactional: changes preview live, **Apply** persists them, and **Cancel** or Back from the root restores the values from before the panel opened.

The portrait settings panel supports the same controller laser as the control grid. Click a row to open a category or activate an action. Adjustable rows show `-` and `+` targets at their left and right edges. Apply and Cancel remain explicit root actions. SteamVR Input navigation remains available as an optional fallback if button delivery becomes functional.

The panel contains Appearance, Placement, independent Left/Right Controller calibration, and Glance and Controls categories. Reset Position and Reset Hand are explicit row actions. Reset All requires a second confirmation click. Glance preview scale and radial-menu long-press duration are persisted with the overlay settings. `Ctrl+Alt+S` is a development fallback for opening the panel.

## Gesture decision

The pose-only wrist gesture, its progress indicator, and thumbstick long-press control-grid entry are disabled. The dashboard is sufficient for the current product direction and avoids gesture false positives, ergonomic tuning, and dependence on experimental overlay-global input. Revisit this work only if extended use demonstrates a need that the dashboard cannot satisfy.

## Physical validation result and remaining work

A Steam Frame/VRLink test found that neither controller thumbstick click advanced the state machine, while the keyboard shortcuts worked. An explicit SteamVR Input experiment identified the running process with the registered application key and shipped a `frame_controller` binding for both thumbsticks, calibration buttons, axes, grips, and triggers. SteamVR accepted the action manifest and binding but did not deliver commands in that physical test.

Follow-up research established that overlay-global bindings can suppress a game's bindings that use the same sources, while even maximum public priority cannot override dashboard focus. Because the approved dashboard makes global shortcuts unnecessary, PhoneCast no longer initializes or submits the broad action set. The manifest and action-reading implementation remain dormant for controlled research only. Ordinary use does not require experimental overlay input overrides.

Keyboard-driven transitions and live, quick reveal were physically confirmed. Left/right keyboard placement selection now also selects the hand used by the next keyboard-driven Glance preview. This is not placement-mode cycling: only the Glance state temporarily attaches a 55%-scale copy of the live phone panel to the selected hand. The 55% preview size and offsets remain unapproved.

- Dashboard icon, panel, and controls are physically approved on Windows/VRLink.
- Defer extended dashboard close/input-release, game-transition, sleep/wake, and receiver-restart testing to ongoing use.
- Validate native ARM64 dashboard lifecycle separately when that backend is active.
- Confirm the disabled wrist and thumbstick gesture paths never activate during ordinary use.
- Tune the 55% preview scale and controller offsets per hand during later interaction testing.
- Verify all four transitions and immediate reveal with live video over extended use.
- Decide whether Expanded should later auto-dismiss and, if so, choose an inactivity interval from headset testing.

Sprint 6's remaining manual UI validation does not block Sprint 7. The project owner chose to gain broader interaction time by proceeding with functional VR-to-phone interaction and recording UI issues as focused follow-up work.
