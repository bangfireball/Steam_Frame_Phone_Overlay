# Frametop research and PhoneCast applicability

Research snapshot: [`DeeJanuz/frametop`](https://github.com/DeeJanuz/frametop) at commit [`ca9492be000a24f8e539607a02ee5d12f41801a6`](https://github.com/DeeJanuz/frametop/tree/ca9492be000a24f8e539607a02ee5d12f41801a6) on the `main` branch.

Scope: native Steam Frame display surfaces, texture transport, controller interaction, placement, game coexistence, lifecycle, and ideas that could reduce risk in PhoneCast. The repository was source-reviewed but was not installed or run as part of this research.

## Executive summary

Frametop is not a phone-mirroring implementation. It is a complete local KDE desktop for Steam Frame, built around a custom nested Wayland compositor. It therefore does not provide reusable Android capture, H.264 transport, pairing, V4L2 decoding, remote Android input, or notification code.

It does, however, contain several valuable Steam Frame findings:

1. **The highest-value technical lead is direct DMA-BUF import into SteamVR.** Frametop passes KWin's Linux DMA-BUFs to OpenVR through `IVRIPCResourceManagerClient::ImportDmabuf`, then submits a `TextureType_SharedTextureHandle`. This avoids the CPU RGBA conversion and Vulkan staging upload that PhoneCast currently uses on Steam Frame.
2. **Frametop has a practical dashboard-closed interaction policy.** It leaves controllers with a running VR game, predicts when a controller ray is aimed at one of its panels, and enables `MakeOverlaysInteractiveIfVisible` only for that panel and for a short linger interval. This is a promising alternative to PhoneCast's current dashboard-only interaction, but it still needs PhoneCast-specific game-input testing.
3. **Its panel manipulation details are directly useful for Sprint 14.** These include a separate move bar, depth adjustment during a drag, safe release handling when the pointer leaves a panel, device-loss cleanup, controller-click stabilization, wrist/head pinning, and controls that appear only when approached.
4. **Its frame scheduling is more sophisticated than a fixed UI loop.** It aligns work with SteamVR display timing, reduces updates for hidden/out-of-view static surfaces, and keeps dynamic/video surfaces at full rate.
5. **It confirms that SteamVR's built-in keyboard is awkward for room overlays on Frame.** Frametop found that the Steam keyboard is mounted into the dashboard scene and built its own overlay keyboard. This supports PhoneCast's existing Sprint 14 requirement to retain a PhoneCast-owned fallback.

The recommended outcome is **not** to adopt Frametop's desktop architecture. PhoneCast should preserve its current Core/VR/platform boundaries and treat Frametop as a reference for several bounded native-backend experiments.

## 1. How Frametop is implemented

### 1.1 Local desktop rather than streamed content

Frametop runs KDE Plasma/KWin locally on Steam Frame. Its central component, `ft-screens`, is a small wlroots Wayland compositor that hosts nested KWin:

```text
KDE applications
      ↓
Nested KWin, one window/output per virtual monitor
      ↓ linux-dmabuf
ft-screens (wlroots compositor)
      ↓ OpenVR shared texture handle
SteamVR regular overlays
```

Each KWin output becomes a separate regular OpenVR overlay. Frametop can therefore give each virtual monitor its own size, resolution, aspect ratio, placement, and curvature. Floating application windows remain inside the same KWin session; Frametop moves each one onto a spare KWin output and shows a crop of that output in another overlay. This preserves desktop clipboard and drag-and-drop behavior.

This architecture is specific to presenting a local Linux desktop. PhoneCast already has a decoded phone frame and should not introduce KWin, Plasma, wlroots, or spare outputs.

Sources:

- [Frametop design: why it uses its own compositor](https://github.com/DeeJanuz/frametop/blob/ca9492be000a24f8e539607a02ee5d12f41801a6/docs/design.md)
- [`screens/compositor.c`](https://github.com/DeeJanuz/frametop/blob/ca9492be000a24f8e539607a02ee5d12f41801a6/screens/compositor.c)
- [Floating-window design](https://github.com/DeeJanuz/frametop/blob/ca9492be000a24f8e539607a02ee5d12f41801a6/docs/floating-windows.md)

### 1.2 Direct compositor-to-SteamVR DMA-BUF path

KWin renders each output into a DMA-BUF. `ft-screens` obtains its dimensions, DRM format, modifier, plane offsets, strides, and file descriptors from wlroots. It then:

1. asks `IVRIPCResourceManagerClient::GetDmabufModifiers` which modifiers SteamVR accepts for XRGB/ARGB;
2. advertises those combinations to KWin through Linux DMA-BUF feedback;
3. imports each new client buffer once with `ImportDmabuf`;
4. caches the returned `SharedTextureHandle_t` by buffer identity;
5. submits a pointer to that handle with `SetOverlayTexture(..., TextureType_SharedTextureHandle, ...)`;
6. retains the displayed wlroots buffer until another buffer replaces it;
7. calls `UnrefResource` when the source buffer is destroyed.

This is substantially different from PhoneCast's current native path:

```text
Current PhoneCast
V4L2 MMAP NV12 → CPU RGBA conversion → mapped Vulkan staging buffer
→ Vulkan image copy/fence → TextureType_Vulkan

Frametop
rendered DMA-BUF → ImportDmabuf → TextureType_SharedTextureHandle
```

The interface is present in the OpenVR 2.15.6 public header, including `DmabufAttributes_t`, `ImportDmabuf`, `GetDmabufModifiers`, and `TextureType_SharedTextureHandle`. Frametop also reports that the Steam Frame runtime implements it. It remains a comparatively obscure, SteamVR-specific interface rather than a portable Linux graphics contract.

Sources:

- [`screens/vr.cpp`](https://github.com/DeeJanuz/frametop/blob/ca9492be000a24f8e539607a02ee5d12f41801a6/screens/vr.cpp)
- [`screens/vr.h`](https://github.com/DeeJanuz/frametop/blob/ca9492be000a24f8e539607a02ee5d12f41801a6/screens/vr.h)
- [`screens/build.sh`](https://github.com/DeeJanuz/frametop/blob/ca9492be000a24f8e539607a02ee5d12f41801a6/screens/build.sh)
- [OpenVR 2.15.6 header](https://github.com/ValveSoftware/openvr/blob/0924064316de3effbcd1acf1e309182a2deb1c05/headers/openvr.h)

### 1.3 Overlay and interaction model

Frametop creates a regular overlay for each screen or floating window and separate small overlays for controls such as move, resize, curve, roll, reset, dock, and close. It configures mouse-style overlay input and discrete scroll events, then translates OpenVR events into a wlroots seat for KWin.

Important implementation details include:

- OpenVR mouse Y coordinates are converted from bottom-left to the desktop's top-left convention.
- Floating windows use `SetOverlayTextureBounds` to crop a larger KWin output without copying its pixels.
- A panel's controls are independent overlays because OpenVR 2.15.6 does not provide child/relative overlay transforms; Frametop recomputes their absolute or tracked-device-relative transforms when the panel moves.
- The move bar rigidly attaches the panel to the pressing tracked device. Scroll while held moves the panel toward or away from the user's head, with bounded distance.
- Wrist and head pins use `SetOverlayTransformTrackedDeviceRelative`; room placement uses standing-space absolute transforms.
- Controls are invisible until a controller ray approaches them, then linger briefly and fade.

### 1.4 Coexistence with VR games

Frametop detects a VR scene application with `IVRApplications::GetCurrentSceneProcessId()`.

Its default product policy is conservative: it can hide or pause the desktop during VR games so the game receives the headset's resources and controller input. It can also leave panels visible over a game. When controllers should remain with the game, Frametop does not keep `MakeOverlaysInteractiveIfVisible` enabled globally. Instead it:

1. reads tracked controller poses;
2. derives the actual laser pose from the render model's `tip` component;
3. tests that ray against the panel, its controls, and popups;
4. enables the interactive-overlay flag only while aimed at a panel;
5. keeps it enabled during a press/drag and for roughly 0.3 seconds after the ray leaves;
6. disables it again so controller input returns to the scene application.

The source records a Frame-specific detail: the controller's render-model tip points about 40 degrees below the controller pose's forward axis. Frametop uses `IVRRenderModels::GetComponentState`; it reports that `GetComponentStateForDevicePath` without an input-source handle failed during VR games.

This is useful supporting evidence for native cross-game overlays and direct interaction, but PhoneCast has not reproduced this behavior. It does not close PhoneCast's pending standalone VR-scene coexistence test.

### 1.5 Frame pacing and resource policy

Frametop does not redraw all desktop surfaces at full rate:

- focused panels receive every display frame;
- other visible panels default to 15 FPS;
- hidden, behind-user, or paused panels default to 1 FPS;
- large, repeatedly damaged surfaces are classified as video and stay at full rate;
- frame callbacks are scheduled from `GetTimeSinceLastVsync` and the HMD display frequency rather than a drifting timer;
- hidden static content incurs no new buffer commits.

This is effective for a desktop compositor because Frametop controls the producer's Wayland frame callbacks. PhoneCast cannot directly apply that mechanism to Android MediaProjection. The general policy—visible/interactive work at full rate, hidden UI work reduced, no catch-up bursts—is still reusable.

### 1.6 Lifecycle and packaging

Frametop normally starts the desktop from Steam's local “Launch a program” flow. The desktop process is placed in a transient user systemd unit, while supporting pointer, power, gaze, and input components use user services tied to `steamvr.service` where appropriate.

A notable startup safety measure is that SteamVR clients first initialize as `VRApplication_Background`. Only after that succeeds do they shut down that connection and initialize as `VRApplication_Overlay`. Frametop reports that direct overlay initialization can start `vrserver` when SteamVR is absent, and that a server started from its container failed to find the headset. Its services also use `Requisite=steamvr.service` so a failed runtime start does not lead a helper to create a broken replacement runtime.

PhoneCast's Sprint 12 launcher already refuses to start SteamVR when it is intentionally stopped. Frametop's two-stage OpenVR probe remains useful defense-in-depth for future native lifecycle hardening.

Frametop does not provide a better replacement for PhoneCast's registered manifest, single-instance IPC, persistent pairing, or dashboard recovery. Its desktop launch flow has different goals.

## 2. What PhoneCast can reuse

### 2.1 High-value experiment: V4L2 DMA-BUF to OpenVR shared handle

This is the clearest new opportunity.

PhoneCast should consider a small, isolated Steam Frame experiment behind the existing platform boundary:

```text
V4L2 capture buffer
      ↓ exported DMA-BUF, if supported
IVRIPCResourceManagerClient::ImportDmabuf
      ↓
TextureType_SharedTextureHandle
      ↓
PhoneCast regular overlay
```

Potential benefits:

- remove NV12-to-RGBA CPU conversion;
- remove the CPU RGBA frame allocation/copy;
- remove the mapped Vulkan staging copy and synchronous upload fence;
- reduce memory bandwidth, CPU use, and native renderer complexity.

Important unknowns and blockers:

- PhoneCast currently allocates V4L2 output and capture queues with `V4L2_MEMORY_MMAP`; it does not export capture buffers.
- Frametop imports compositor-produced XRGB/ARGB DMA-BUFs, not Qualcomm decoder-produced NV12.
- SteamVR must report compatible DRM formats/modifiers. Frametop does not prove that NV12 is accepted as an overlay texture.
- The Frame decoder advertised RGBA in the earlier Sprint 11 probe, but selecting it successfully, identifying the exact DRM fourcc/channel order, and measuring decoder cost remain untested.
- Multi-planar V4L2 layout, offsets, strides, modifiers, color conversion, and visible-versus-coded crop must be correct.
- Buffer ownership is critical. A decoder capture buffer cannot be requeued while SteamVR may still sample it. Frametop keeps the current wlroots buffer locked until a successor is submitted and relies on the graphics stack's synchronization. PhoneCast needs an explicit, tested queue/release strategy for V4L2 buffers.
- Orientation changes, reconnect, teardown, and `UnrefResource` ordering need physical validation.
- `IVRIPCResourceManagerClient` is SteamVR-specific and should remain inside `platform/openvr` / `platform/steam-frame-arm64`, with the current Vulkan path retained as a fallback.

Because PhoneCast's existing path is already physically approved at about 30 FPS with low process CPU and stable memory, this is an optimization experiment rather than a Sprint 12 blocker.

### 2.2 Direct interaction over games without a permanent global claim

PhoneCast currently keeps the visible phone view-only while the dashboard is closed because a permanently enabled `MakeOverlaysInteractiveIfVisible` took controller input from the game. Frametop demonstrates a narrower policy:

- compute controller laser rays independently of OpenVR pointer focus;
- enable interaction only when a ray intersects the phone or its toolbar;
- retain it during a press/drag;
- use entry/exit margins and a short linger to avoid flicker;
- disable it immediately after leaving so the game regains input.

This could materially improve PhoneCast's native UX. It should be treated as an opt-in Sprint 14 experiment, not copied directly into current behavior. Acceptance must include real-game testing for accidental input loss, both controllers, dashboard transitions, trigger release, tracking loss, and the Frame controller's actual laser-tip transform.

### 2.3 Safer panel drag and release behavior

Frametop has patterns that map directly to Sprint 14's input-safety acceptance criteria:

- end a drag when its tracked device is deactivated;
- treat a release delivered on any owned panel as ending that device's drag;
- keep a press logically grabbed while the ray briefly leaves;
- use an invisible “release catcher” while a held pointer is between panels;
- provide a fallback release path and stuck-press reconciliation;
- keep move controls separate from content input;
- adjust depth along the head-to-panel line while moving, rather than world vertical;
- clamp depth to safe bounds.

PhoneCast should reuse the state-machine ideas in its portable interaction layer and keep pose/event collection in the OpenVR backend. The invisible catcher is OpenVR-specific and requires care because `MakeOverlaysInteractiveIfVisible` can affect game input.

### 2.4 Controller-click stabilization

Frametop's controller click filter delivers trigger-down immediately but suppresses small pointer motion until the ray moves beyond a threshold. If trigger-up occurs within the threshold, the click is completed at the original press point. Once the threshold is crossed, the interaction becomes a normal drag and does not revert to a click. There is no hold timer.

This could help PhoneCast with release wobble on small Android targets. PhoneCast should adapt it to normalized/angular or physical panel distance rather than copying Frametop's KDE logical-pixel threshold. It must preserve Android long-press and the existing live drag-scroll behavior.

Source: [controller click filter](https://github.com/DeeJanuz/frametop/blob/ca9492be000a24f8e539607a02ee5d12f41801a6/screens/controller-click.h) and [design note](https://github.com/DeeJanuz/frametop/blob/ca9492be000a24f8e539607a02ee5d12f41801a6/docs/controller-click-stability.md).

### 2.5 Native-style lower controls and placement

Frametop's panels already use the interaction style planned for PhoneCast Sprint 14:

- a control strip beneath content;
- a distinct grab bar;
- separate resize/close/reset controls;
- hover/active visual states;
- depth adjustment while holding the bar;
- room, head, and wrist/controller placement;
- wrist-facing alpha fade;
- controls hidden when not approached.

PhoneCast should borrow the interaction rules and ergonomics, not Frametop's rendered assets or desktop-specific implementation. PhoneCast's existing portable placement/settings model should remain authoritative.

### 2.6 Keyboard fallback

Frametop tried `ShowKeyboardForOverlay` and found that the Steam keyboard is Steam's dashboard panel: with the dashboard closed it was not drawn normally, and forcing its transform did not produce stable independent behavior. Frametop therefore renders its own keyboard overlay and forwards key events to its compositor.

PhoneCast should still test the public SteamVR keyboard API itself, but this evidence increases the likelihood that the Sprint 14 portable PhoneCast-owned keyboard fallback will be necessary. Frametop's keyboard is a US desktop key matrix and is not directly suitable for Android text entry, IME composition, Unicode, or mobile input semantics.

### 2.7 Update resilience

Frametop explicitly tracks dependencies on SteamOS-shipped KWin, gamescope, SteamVR interfaces, and undocumented behavior in an `update-check.py` tool. PhoneCast has fewer host integrations, but any adoption of `IVRIPCResourceManagerClient`, V4L2 DMA-BUF assumptions, controller render-model details, or vrserver-local interfaces should add startup capability checks and clear fallback behavior.

## 3. What should not be copied

- **The nested KDE/wlroots desktop architecture:** unrelated to a decoded phone texture and far too heavy for PhoneCast.
- **The gaze, hand tracking, Bluetooth, input relay, virtual controller, VNC, KWin scripting, and floating-output systems:** useful for Frametop's desktop goals, not for PhoneCast's current product.
- **The undocumented vrserver WebSocket input feed:** Frametop uses it for a pause gesture without taking buttons from games. It is firmware/runtime fragile, exposes raw controller state, and the game still receives the gesture. PhoneCast should prefer its dashboard and narrowly scoped supported OpenVR input paths.
- **Private or firmware-coupled interfaces without fallback:** direct DMA-BUF import is worth testing because the public OpenVR header exposes it, but it still needs runtime capability detection. Other private Steam/SteamVR techniques should not become required product paths.
- **Frametop's game-pause policy:** PhoneCast is intentionally useful over games. It may reduce hidden-state work, but it should not stop the phone stream merely because a scene application starts.
- **Direct source copying without attribution:** Frametop is MIT-licensed and PhoneCast is also MIT-licensed, so reuse is legally compatible. If substantial code is copied, retain DeeJanuz's copyright and MIT notice as required. Reimplementing the bounded behavior against PhoneCast's interfaces may be cleaner.

## 4. Comparison with current PhoneCast

| Area | Frametop | PhoneCast implication |
|---|---|---|
| Content source | Local KWin desktop DMA-BUFs | No Android/codec/networking reuse |
| Native texture path | DMA-BUF import to OpenVR shared handle | Strong lead for a future zero-copy decoder path |
| Current proven fallback | Not PhoneCast's concern | Keep the physically approved CPU RGBA + Vulkan path |
| Placement | Room, head, wrist; grab/resize/curve/roll | Interaction patterns fit Sprint 14; preserve PhoneCast model |
| Game detection | `GetCurrentSceneProcessId()` | Useful for policy/diagnostics, not proof of PhoneCast coexistence |
| Game input coexistence | Aim-gated interactive flag and optional pause | Promising direct-interaction experiment |
| Click safety | Movement threshold, release catcher, device-loss cleanup | Reusable state-machine ideas |
| Keyboard | Custom overlay after Steam keyboard limitations | Supports PhoneCast fallback planning |
| Scheduling | Vsync-aligned attention/damage-based callbacks | Reuse policy concepts, not Wayland mechanics |
| Launcher/lifecycle | Local desktop launcher and user services | PhoneCast's current manifest, installer, IPC, pairing, and dashboard recovery remain better matched |
| Security | Local desktop; no phone stream trust boundary | Does not help encrypted pairing or remote-control security |

## 5. Recommended follow-up order

1. **Finish Sprint 12 physical validation first.** Frametop research does not replace PhoneCast's dashboard `+`, sleep/wake, reconnect, rotation, shutdown, notification, or standalone VR-scene test matrix.
2. **Add a bounded DMA-BUF feasibility task after the current native baseline is stable.** First query SteamVR's accepted DRM formats/modifiers and verify V4L2 export support without changing the production path.
3. **If compatible, prototype one imported buffer path with strict ownership logging.** Compare it against the current renderer for CPU, memory bandwidth, submit time, latency, stability, orientation changes, and teardown.
4. **During Sprint 14, prototype aim-gated direct interaction.** Use the render-model laser tip, hysteresis/linger, press ownership, device-loss cleanup, and a guaranteed fallback to dashboard-only input.
5. **Port the click-stabilization and release-safety concepts into portable tests.** Cover tap, long-press, drag, scroll, focus loss, tracking loss, dashboard close, and cross-surface release.
6. **Plan for a PhoneCast-owned keyboard.** Test SteamVR's public keyboard once, but do not make Sprint 14 depend on it working outside the dashboard.
7. **Retain capability checks and fallback paths.** A SteamOS or SteamVR update must be able to disable DMA-BUF/direct-interaction optimizations without breaking the established Vulkan/dashboard experience.

## 6. Evidence classification

### Source-reviewed in Frametop

- The custom wlroots/KWin compositor architecture.
- DMA-BUF modifier negotiation, import, shared-handle caching, and `SetOverlayTexture` submission.
- Scene-process detection and conditional interaction policy.
- Laser-tip lookup, grab/depth/pin behavior, click stabilization, and release handling.
- Vsync-aligned, attention-based frame callbacks.
- Frametop's custom keyboard and documented reason for not using Steam's keyboard.
- Startup probing as a background app before overlay initialization.

### Reported by the Frametop author, not independently reproduced here

- Operation on Steam Frame hardware.
- Direct DMA-BUF import functioning in the installed SteamVR runtime.
- Panels visible/usable under Frametop's documented VR-game modes.
- Frame-specific controller-tip and Steam keyboard behavior.
- Resource/performance improvements described in Frametop documentation.

### Already physically established by PhoneCast

- Native ARM64 OpenVR regular overlays.
- Qualcomm V4L2 H.264 decode to NV12.
- CPU NV12-to-RGBA conversion and reusable Vulkan `SetOverlayTexture` rendering at approximately 30 FPS.
- Dashboard, placement, remote Android interaction, and notification features in initial native use.
- Stable memory and smooth local flat-game coexistence for the measured Vulkan run.

### Still unknown for PhoneCast

- Standalone VR-scene coexistence in both launch orders.
- Whether SteamVR accepts a PhoneCast decoder buffer's exact DRM format/modifier.
- Whether the Qualcomm decoder exports usable DMA-BUF capture buffers with safe synchronization.
- Whether direct dashboard-closed PhoneCast interaction can coexist with real game input reliably.
- SteamVR keyboard behavior for PhoneCast's Android text-entry needs.

## Conclusion

Frametop does not offer a reusable phone-casting pipeline, but it is a strong reference implementation for native Steam Frame overlay mechanics. Its most important contribution is proof-of-implementation for OpenVR DMA-BUF import, which may eventually remove PhoneCast's remaining application-side decoder-to-CPU-to-Vulkan copies. SteamVR's internal handling of an imported buffer is not established by this review. Its aim-gated interaction, laser-tip correction, drag/release safety, click stabilization, lower control strip, keyboard fallback, and adaptive scheduling are also useful inputs to Sprint 14.

The safe direction is incremental: keep PhoneCast's approved Vulkan renderer and dashboard-first UX, complete the pending native lifecycle tests, then evaluate Frametop's ideas as isolated optional improvements behind the existing platform interfaces and fallbacks.
