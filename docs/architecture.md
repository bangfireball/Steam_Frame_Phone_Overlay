# PhoneCast VR architecture

## Sprint 1 baseline

The receiver is divided into portable contracts, application orchestration, and an OpenVR adapter:

```text
GeneratedVideoSource (Core)
          |
          v
Receiver orchestration (apps/receiver)
          |
          v
IOverlayRenderer (VR contract)
          |
          v
OpenVrOverlayRenderer (platform/openvr)
          |
          v
SteamVR compositor
```

## Dependency rules

- `core` contains configuration, logging, frame types, generated video, and future transport/decoder/input contracts. It does not include OpenVR, Win32, DirectX, or operating-system headers.
- `vr` defines renderer-facing VR contracts and depends only on Core frame types.
- `apps/receiver` coordinates a video source and overlay renderer through interfaces.
- `platform/openvr` is the only production component that includes `openvr.h`.
- The executable composition root selects concrete implementations.
- Platform packaging may depend on Windows x64 or Linux ARM64 details; reusable logic may not.

## Current interfaces

- `IVideoSource` supplies decoded or generated `VideoFrame` objects.
- `IVideoDecoder` defines the future encoded-packet boundary; Sprint 1 has no codec.
- `INetworkTransport` defines a future byte transport; Sprint 1 opens no sockets.
- `IInputProvider` and `IRemoteInputSender` define normalized platform-independent input boundaries. The Windows TCP server implements the sender side; renderers publish input without owning the transport.
- `IOverlayRenderer` owns VR startup, frame submission, event pumping, and shutdown.
- `ILogger` receives severity, subsystem, and message fields.

Interfaces are deliberately small. They will be revised when the first real transport and decoder provide concrete requirements.

## Frame ownership and format

`VideoFrame` owns tightly packed RGBA8 CPU memory. The generated source produces a moving vertical band and horizontal progress line. Sprint 1 initially uploaded each Linux frame with `SetOverlayRaw`; the current Linux backend stages the same portable frame into reusable Vulkan images without exposing graphics handles through Core.

The retained CPU frame boundary supports portable decoding and tests. Direct decoder-surface/GPU import remains a later measured optimization inside the platform boundary.

## Runtime lifecycle

1. Parse and validate command-line configuration.
2. Initialize OpenVR as `VRApplication_Overlay`.
3. Register the adjacent application manifest when available.
4. Create and configure an HMD-relative regular overlay.
5. Generate and submit frames at the configured update rate.
6. Poll overlay and system quit events.
7. Hide/destroy the overlay and call `VR_Shutdown`.

`Receiver::Stop` is idempotent. The OpenVR renderer also stops in its destructor. `VREvent_ProcessQuit` is informational about an arbitrary VR process and is deliberately not treated as a receiver shutdown request; doing so caused PhoneCast to exit when a VR scene application transitioned. Only explicit runtime/driver quit events stop the overlay.

## Platform mapping

### Windows x64

Uses Valve's OpenVR client DLL and copies it beside `phonecast-receiver.exe`. No Win32 API is used by Core.

### Steam Frame Linux ARM64

Builds OpenVR from pinned source and links it statically. The same backend uses the manifest's required `binary_path_linux_arm`. Sprint 1 cross-builds this path; native visual validation remains hardware-dependent.

## Sprint 3 streaming path

```text
Android MediaProjection → MediaCodec AVC → framed TCP
    → Windows receiver → Media Foundation H.264 decoder → RGBA8 → Win32 preview
```

The protocol serializer remains in Core and has no socket or operating-system headers. Android and the Windows receiver implement the same fixed, network-byte-order framing. The Windows decoder and desktop window are isolated under `platform/windows-x64`.

The desktop preview is a validation target, not the Sprint 4 VR rendering path. It currently converts Media Foundation NV12 output to CPU RGBA8. Steam Frame decoding and efficient decoder-to-compositor texture sharing remain separate backend work.

## Sprint 4 Windows VR path

```text
Android MediaProjection → MediaCodec AVC → framed TCP
    → TcpVideoServer → Media Foundation H.264 decoder → CPU RGBA8
    → reusable D3D11 texture → OpenVR SetOverlayTexture → VR compositor
```

The desktop and VR streaming executables share `TcpVideoServer` and `MfH264Decoder`. The desktop preview remains an independent diagnostic target. On Windows, `OpenVrOverlayRenderer` creates one D3D11 texture per stream resolution and updates it instead of repeatedly calling `SetOverlayRaw`; Linux ARM64 now uses its separate reusable Vulkan path described under Sprint 11.

The D3D11 device must be created on the DXGI adapter returned by OpenVR's `GetDXGIOutputInfo`. Using Windows' default adapter caused successful API calls but an invisible overlay on the multi-GPU validation PC. Each `UpdateSubresource` is followed by `ID3D11DeviceContext::Flush`; without that flush, the generated animation advanced only about once every five seconds through VRLink. With both corrections, the generated animation was visibly smooth and the physical Android screen appeared in-headset.

`OverlayController` contains platform-independent, bounded changes for visibility, width, X/Y offset, distance, opacity, placement mode, and reset. Placement supports head-, standing-world-, left-controller-, and right-controller-locked transforms. The Windows executable maps global Ctrl+Alt key combinations to those actions.

For world placement, the OpenVR backend snapshots the current HMD-relative panel into SteamVR standing space, so switching modes does not move the panel to the room origin. Controller modes use role-based tracked-device poses and independent left/right calibration profiles. Each hand persists distance, height, lateral offset, tilt, yaw, scale, and one of four orientation policies: controller-relative, face-user, world-upright, or wrist-style. Controller-relative and wrist modes inherit controller rotation; face-user aims directly at the HMD; world-upright aims horizontally while preserving standing-space up. If controller tracking is temporarily invalid, the compositor retains the last valid transform and updates resume when tracking returns.

The OpenVR backend provides an in-headset calibration mode: either controller's menu button selects that hand and enters calibration, and the selected button toggles calibration. Axis motion adjusts position, grip plus axis adjusts yaw/tilt, trigger plus axis adjusts scale/distance, pad click cycles orientation, and grip plus pad click resets the active hand. Changes render immediately and are published through `IOverlayRenderer::TakeSettingsUpdate` for portable persistence after adjustment ends. OpenVR mouse input also exposes a horizontal handle below the phone image. Trigger-dragging that handle repositions the panel, and release converts its current pose to a world anchor without claiming the controller grip button. No OpenVR types enter the portable settings model.

The Windows-hosted overlay disappears when a headset-native standalone game takes over; this is a compositor/backend boundary rather than a placement-model failure and remains native Sprint 11 work. Sprint 5.1 controller calibration is implemented, covered by automated tests, and closed for its implemented scope. Extended ergonomic tuning remains explicitly deferred to ongoing headset use rather than being represented as physically complete.

Sprint 6 adds a portable `GlanceController` with Hidden, Glance, Expanded, and Pinned session states. Glance derives a small controller-locked presentation without modifying persisted placement; Expanded and Pinned restore the user's normal settings. Earlier experimental SteamVR Input actions and the long-press control grid remain in dormant code, but the supported dashboard-only configuration neither initializes nor submits the broad overlay-global action set because it could suppress game bindings. The application still owns transitions, visibility, presentation, and transactional `SettingsMenuController` state; the OpenVR backend renders panels and translates dashboard pointer events into portable commands. Numeric settings expose portable normalized slider values in addition to incremental controls. The OpenVR adapter world-locks the settings surface and implements its independent move handle without placing compositor transforms in the portable model. Video decoding continues while hidden and while menus are open.

The OpenVR backend also creates a best-effort dashboard main/thumbnail pair. Its static PhoneCast icon and state-driven control panel expose portable Show/Hide, Glance, Pin, Settings, and placement actions. The dashboard also presents receiver-owned remote-control readiness derived from a bounded Android status message; Core carries the two booleans while Android remains authoritative for permission enforcement. Dashboard creation failure does not prevent the regular overlay from starting. OpenVR owns dashboard placement and pointer delivery; no dashboard handles or SteamVR-specific concepts enter the portable state model. The panel is updated only when visibility changes rather than at video cadence. The dashboard is physically approved on Windows/VRLink and is the supported in-headset entry point. Wrist-pose and thumbstick long-press menu gestures are disabled at runtime; native ARM64 dashboard lifecycle remains to be tested. See `docs/glance-mode.md`.

`OverlaySettingsStore` persists the portable placement, appearance, Glance preview scale, and radial long-press duration in a versioned text file. The stream receiver stores it under `%LOCALAPPDATA%\PhoneCastVR` by default and accepts `--settings` for an explicit path. Stream aspect-ratio adjustment remains presentation-only and is not written over the user's base width.

Media Foundation may report an aligned coded width that exceeds the phone's visible width. The decoder now allocates using coded dimensions while converting and exposing only the original visible dimensions. This is intended to remove the previously observed right-edge green padding and requires physical revalidation.

The current path still performs decoder NV12 → CPU RGBA → D3D11 upload. Direct Media Foundation/DXGI surface conversion and synchronization remain performance work; platform handles have not been added to Core merely to anticipate that optimization.

## Sprints 7–8 interaction path

```text
OpenVR overlay mouse/scroll events
    → OverlayInteractionController (normalized top-left coordinates)
    → IOverlayRenderer::TakePointerEvent
    → IRemoteInputSender / TcpVideoServer
    → REMOTE_INPUT protocol message
    → Android NetworkStreamer
    → opt-in RemoteControlAccessibilityService
    → dispatchGesture / Android Back
```

The renderer, protocol, and Android injector remain separate. OpenVR coordinates and Android framework classes do not enter Core. While the SteamVR dashboard is open, trigger controls the phone, trigger-dragging the separate bottom handle provides direct overlay placement without an ambiguous grip chord, and a lower-left `<` target emits Android Back. The persistent phone overlay deliberately omits `MakeOverlaysInteractiveIfVisible`: that OpenVR flag activates system-wide laser mode and was physically observed to withhold hand input from the running game whenever the phone was visible. The phone is therefore view-only while the dashboard is closed. Android uses one complete gesture for taps and stationary long presses, then switches moving pointers to serialized `StrokeDescription.continueStroke` segments so held-trigger drag scrolling updates before release. The service is separately user-enabled, does not retrieve window content, and can be disabled while capture remains active. See `docs/remote-control.md`.

## Sprint 9 notification path

```text
Android NotificationListenerService
    → opt-in privacy and package filters
    → bounded NOTIFICATION message on the active stream connection
    → portable NotificationEvent
    → transient IOverlayRenderer notification card
    → OpenVR head-relative card
```

Notification access, filtering, and redaction stay on Android. Content is hidden by default; the receiver renders only the already-filtered event and does not persist it. Notification messages use a separate bounded sender queue and receiver handling that does not alter H.264 sequence/keyframe state. The renderer contract contains no Android or OpenVR types. The OpenVR implementation uses a separate short-lived overlay, avoiding texture composition with the phone stream and preserving the phone's Hidden/Glance/Expanded/Pinned state. Selecting a card with the dashboard laser requests the portable application to show the full phone and returns an opaque action token to Android. Android alone retains and invokes the original notification `PendingIntent`; platform Intent data never enters Core. The phone footer also exposes a renderer-local close request at its lower-right, parallel to Android Back at lower-left.

See `docs/notifications.md` for privacy, security, PC-hosted physical approval, and native ARM64 validation still pending.

## Sprint 10 measurement path

The Windows VR receiver can write one-second CSV samples without adding Windows APIs to Core. A Windows process sampler reports PhoneCast CPU and memory; the OpenVR adapter translates `Compositor_FrameTiming` into a portable `VrPerformanceStats` snapshot; and the application combines those values with existing transport, decode, upload, queue, and startup counters. OpenVR values describe the active scene/compositor and are not falsely attributed as PhoneCast-only GPU utilization. External high-speed-camera testing remains required for glass-to-glass latency because Android and Windows timestamps do not share a calibrated clock.

Android capture profiles remain entirely sender-side. Battery Saver, Standard, and Quality select encoder dimensions, FPS, and bitrate bounds before MediaCodec configuration; no Windows or OpenVR type enters that model. Standard preserves the accepted 1280/30 baseline, while the other values remain subject to physical measurement.

## Sprint 11 native Steam Frame path

```text
Android MediaProjection → MediaCodec AVC → framed TCP
    → shared TcpVideoServer → Linux stateful V4L2 H.264 M2M decoder
    → NV12/NV12M → CPU RGBA → Vulkan staging upload
    → reusable double-buffered images → OpenVR SetOverlayTexture
```

`TcpVideoServer` now lives under `platform/network` and supplies WinSock or POSIX socket mechanics behind the same receiver-facing API. Protocol parsing, bounded queue/keyframe recovery, remote input, notification actions, and Android gate status remain shared. This removes the former accidental dependency between transport behavior and `platform/windows-x64`.

The Linux ARM64 composition root is the same `phonecast-vr-stream-receiver` application used on Windows. CMake selects Media Foundation/D3D11/process sampling on Windows and the Steam Frame V4L2/procfs implementation on Linux. The native application therefore reuses the existing OpenVR dashboard, placement, interaction, notification, persistence, and performance models instead of creating a reduced parallel receiver. Linux uses XDG settings paths and signal-driven shutdown.

The native OpenVR backend loads `libvulkan.so.1` at runtime, enables the instance and device extensions required by OpenVR, selects the compositor's physical device, and retains two device-local RGBA images plus a persistently mapped staging buffer. Each CPU RGBA frame is copied into staging, transferred into the next image, synchronized with a fence, and submitted as `TextureType_Vulkan`. Stream-size changes clear the old compositor texture before recreating resources. Shutdown clears the overlay texture and destroys overlays, calls `VR_Shutdown`, and only then releases Vulkan resources. Vulkan handles remain inside `platform/openvr`; Core's `VideoFrame` stays platform-independent.

`V4l2H264Decoder` discovers a streaming multi-planar H.264 M2M device or accepts an explicit `/dev/videoN`. It uses the stateful decoder source-change lifecycle, MMAP input/capture queues, and linear NV12/NV12M capture. The initial implementation converts into the existing portable `VideoFrame` because that enables full native behavior without introducing an unvalidated graphics interop handle into Core.

Decoder failures no longer leave an authenticated stream permanently receiving without decoding. The portable `DecoderRecoveryController` detects either an explicit decoder error or three consecutive active one-second receive windows with no decoded output. The composition root stops and reopens the platform decoder with bounded backoff (three attempts), while `TcpVideoServer::RequestVideoResync` discards queued prediction frames and requests a fresh keyframe without closing the remote-input connection. Recovery counters are included in console and CSV diagnostics. The policy is automatically tested and ARM64 cross-built; recovery from the observed Qualcomm `EBUSY` failure still requires physical validation.

The V4L2 decode path and Vulkan renderer are physically established on Steam Frame. A measured local-game run sustained approximately 30 FPS with stable memory and no texture-submission or OpenVR frame-drop errors, and the project owner approved the result as smooth and flicker-free. Orientation-driven texture recreation, clean shutdown, longer lifecycle cases, and actual standalone-VR-scene coexistence remain separate physical checks. A future dma-buf/zero-copy path still belongs in the platform renderer/decoder boundary and is optional unless later measurements justify it.

## Sprint 12 standalone launch baseline

Steam Frame packaging remains in the platform boundary. A user-level desktop
entry launches an absolute-path shell wrapper, which first verifies that the VR
runtime is already running and then starts the native composition root. The
wrapper does not start SteamVR. Linux-only `StandaloneRuntime` owns a per-user
file lock and private Unix-domain command socket; a second launch sends a
bounded focus command and exits before creating a decoder, TCP listener, or
OpenVR client.

The resident composition root translates that command into the concrete OpenVR
renderer's `ShowDashboard` operation. Dashboard handles remain OpenVR-private;
the backend periodically validates its main handle and recreates the dashboard
pair if SteamVR discarded it. Pairing-code persistence is also Linux-platform
state and feeds only the existing protocol gate and local dashboard display. No
socket, filesystem-permission, desktop-entry, or OpenVR launcher type enters
Core or the portable VR models.

See `docs/standalone-ux.md` for packaging and validation boundaries.

## Sprint 13 optional playback audio

Default-off Android AudioPlaybackCapture sends negotiated 48 kHz stereo PCM S16LE
over the existing paired stream. Separate bounded audio queues, epochs/sequences,
and a shared sender wake mechanism preserve video prediction/notification behavior.
Core owns `IAudioOutput`, `AudioPlayback` worker/gain/retry semantics and bounded
`AudioVideoQueue` coalescing; no platform APIs enter these models. Actual decoded
picture PTS is retained in `VideoFrame` through both MF and V4L2.

Windows implements shared-mode WASAPI; Steam Frame runtime-loads libpulse for
mixed PulseAudio/PipeWire output. OpenVR supplies only an optional output-device
hint; the composition root selects a backend/route and coordinates connection
changes and transactional portable mute/volume/output settings (store version 4).
Audio failures remain non-fatal to video. Queues and sound devices never block the
VR render loop. Video playout retains at most eight pictures with individual
100 ms arrival-based deadlines; only due pictures are coalesced. Capacity pressure
forces presentation progress rather than silently renewing the wait. This fixes
the reproduced three-picture audio-clock starvation without enlarging audio
buffers or changing the renderer. Receiver diagnostics expose queue/deadline/
overflow/hold and estimated media-skew observations. See
[`phone-audio.md`](phone-audio.md) for timing policy, regression evidence, and the
pending physical acceptance matrix; initial audibility is user-reported, but the
corrected audio-enabled video cadence still requires headset retesting.

## Deferred work

Encrypted pairing, automatic discovery, extended ergonomic tuning of controller-placement defaults, physical standalone native-overlay validation, native decoder-to-GPU surface sharing, and process-attributed GPU utilization remain deferred. The custom TCP transport is subject to head-of-line blocking and must be measured on real Wi-Fi before it is treated as a long-term choice.
