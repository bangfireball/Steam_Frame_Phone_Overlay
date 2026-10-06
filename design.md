# PhoneCast VR — Design & Implementation Plan

## 1. Project Overview

PhoneCast VR is a VR utility that allows a user to view and eventually interact with their Android phone from inside VR while another VR game or application is running.

The long-term target is **Steam Frame running standalone with no PC required**.

The initial MVP may use a Windows PC running SteamVR as an intermediary because this provides an easier development and debugging environment. However, the architecture MUST be designed so that the networking, streaming, protocol, phone integration, and most VR functionality can later be reused on Steam Frame ARM64.

The intended evolution is:

```text
Initial MVP

Android Phone
      │
      │ Wi-Fi / LAN
      ▼
Windows PC
      │
      │ OpenVR Overlay
      ▼
SteamVR
      │
      ▼
Steam Frame / VR Headset
```

Eventually:

```text
Standalone

Android Phone
      │
      │ Wi-Fi / LAN
      ▼
Steam Frame
      │
      │ Native ARM64
      ▼
SteamVR / VR Compositor
      │
      ▼
Running Standalone VR Game
        +
Persistent Phone Overlay
```

The Windows implementation is therefore a **development platform**, not the fundamental architecture of the application.

---

# 2. Core Product Goals

PhoneCast VR should eventually provide:

- Live Android screen mirroring inside VR.
- Persistent overlay while playing VR games.
- Low-latency local-network streaming.
- Adjustable overlay size.
- Adjustable overlay position.
- Adjustable overlay opacity.
- Head-locked positioning.
- World-locked positioning.
- Controller/wrist positioning.
- Hide/show controls.
- VR controller interaction with the phone.
- Remote touch input.
- Scrolling.
- Text input.
- Android notification integration.
- Lightweight notification cards.
- Standalone Steam Frame operation without a PC.

The application should prioritize:

1. Low latency.
2. Low performance impact on the running game.
3. Simple interaction.
4. Portability.
5. Modular architecture.
6. Steam Frame compatibility.

---

# 3. Critical Architectural Rule

Do NOT tightly couple the application to Windows, DirectX, Win32, or a specific video decoder.

Platform-specific functionality must exist behind interfaces.

Conceptually:

```text
PhoneCast
│
├── Core
│
│   ├── Networking
│   ├── Protocol
│   ├── Streaming
│   ├── Pairing
│   ├── Configuration
│   └── Input Mapping
│
├── VR
│   ├── Overlay Model
│   ├── Overlay Positioning
│   ├── Interaction
│   └── Controller Input
│
└── Platform
    │
    ├── Windows x64
    │   ├── OpenVR
    │   └── Video Decoder
    │
    └── Steam Frame ARM64
        ├── OpenVR / OpenXR
        └── Video Decoder
```

Core logic should not know whether it is running on:

- Windows x64
- Linux ARM64
- Steam Frame
- another future platform

Platform-specific code should implement common interfaces.

Suggested conceptual interfaces:

```text
IVideoReceiver
IVideoDecoder
IVideoFrame
IOverlayRenderer
IVRInputProvider
IRemoteInputSender
INetworkTransport
IPlatformServices
ISettingsStore
ILogger
```

Exact naming and implementation language may differ if investigation identifies a better architecture.

---

# 4. Repository Structure

Begin with a structure approximately like:

```text
phonecast-vr/
│
├── README.md
├── design.md
├── docs/
│   ├── architecture.md
│   ├── protocol.md
│   ├── steam-frame.md
│   └── development.md
│
├── android/
│   └── sender/
│
├── core/
│   ├── network/
│   ├── protocol/
│   ├── streaming/
│   ├── input/
│   ├── config/
│   └── logging/
│
├── vr/
│   ├── overlay/
│   ├── positioning/
│   └── interaction/
│
├── platform/
│   ├── windows-x64/
│   └── steam-frame-arm64/
│
├── experiments/
│   └── hello-frame/
│
└── tests/
```

Do not create unnecessary abstractions merely to match this structure.

Modify it where technically justified.

---

# 5. Development Philosophy

Implement the project incrementally.

Every sprint MUST leave the repository in a working state.

Do not attempt to implement future sprint functionality unless it is necessary for the current architecture.

Before major architectural decisions:

1. Investigate the available APIs.
2. Document findings.
3. Identify portability concerns.
4. Choose the simplest solution that preserves the standalone Steam Frame goal.

Avoid premature UI polish.

Performance, architecture, and proof of feasibility come first.

---

# Sprint 0 — Hello Frame

**Status:** `[x] Complete — foundational feasibility accepted by the project owner`

- `[x]` Initial Valve/OpenVR/OpenXR research
- `[x]` Minimal Hello Frame source implementation
- `[x]` Windows x64 build validation
- `[!]` PC SteamVR visual validation not run; no longer required to establish native feasibility
- `[x]` Linux ARM64 cross-build validation
- `[x]` Native Steam Frame deployment, overlay creation, and raw-image validation
- `[x]` User-confirmed visible, head-locked coexistence with a standalone 2D game
- `[x]` Foundational native-overlay feasibility accepted by the project owner
- `[ ]` Follow-up validation: coexistence with a standalone VR scene application

## Recorded Result

**Outcome:** Success for the architectural feasibility gate, accepted 2026-10-02.

Tested on Steam Frame:

- SteamOS build `20260922.6101926` on Linux ARM64;
- native SteamVR `2.17.10`;
- OpenVR SDK/client `2.15.6`;
- `VRApplication_Overlay` initialization;
- regular `IVROverlay::CreateOverlay` overlay;
- generated 640 × 240 RGBA texture uploaded through `SetOverlayRaw`;
- HMD-relative placement approximately one metre forward;
- user-visible overlay while an on-device standalone 2D game was running;
- clean startup and shutdown;
- approximately 16.7 MiB resident memory and 0.9% CPU during an initial sample.

Observed behavior:

- The panel was head-locked and moved with the user, as intended for this prototype.
- The panel was not interactive because Sprint 0 did not configure an input method or implement interaction.
- Steam Frame requires `binary_path_linux_arm` in the application manifest.
- Registering a stable manifest and classifying the application as an overlay is required for reliable coexistence across application transitions.

Remaining limitation:

- The exact original test with another standalone **VR scene application** has not yet been performed. This remains an explicit follow-up risk and must not be represented as tested.

Architecture decision:

- Proceed with Sprint 1.
- Treat native Linux ARM64 OpenVR as the preferred Steam Frame overlay backend unless the VR-scene follow-up exposes a restriction.
- Preserve the platform abstraction and PC-hosted fallback.
- Keep detailed evidence, commands, and unresolved questions in `docs/steam-frame.md`.

## Objective

Determine whether Steam Frame can run a persistent third-party VR overlay over another VR application.

This is the highest-priority technical experiment.

Do NOT begin implementing the complete phone streaming system until this experiment has been investigated and documented.

## 0.1 Research

Investigate current Valve documentation and SDKs for:

- Steam Frame development.
- Linux ARM64 development.
- OpenVR support.
- OpenXR support.
- `IVROverlay`.
- `VRApplication_Overlay`.
- SteamVR compositor behavior.
- standalone Steam Frame applications.
- overlay lifecycle.
- running overlays simultaneously with another VR application.
- ARM64 OpenVR libraries.
- overlay controller input.

Document findings in:

```text
docs/steam-frame.md
```

Include links to primary Valve documentation where appropriate.

Clearly distinguish:

```text
Documented behavior
Tested behavior
Assumed behavior
Unknown behavior
```

Do not treat assumptions as confirmed functionality.

---

## 0.2 Hello Frame Prototype

Create the smallest possible VR overlay.

The overlay should display:

```text
┌──────────────────────────┐
│                          │
│       HELLO FRAME        │
│                          │
└──────────────────────────┘
```

No phone streaming.

No complicated UI.

No networking.

No configuration system unless required.

The application should:

- initialize the appropriate VR runtime;
- create an overlay;
- generate a simple texture;
- display that texture;
- position it approximately one meter in front of the user;
- remain running independently of the VR game.

Prefer OpenVR initially if supported because `IVROverlay` directly models the required behavior.

---

## 0.3 PC Baseline Test

First verify the Hello Frame overlay against PC SteamVR.

Expected result:

```text
SteamVR
│
├── VR Game
│
└── Hello Frame Overlay
```

Confirm that the overlay remains visible while another VR application is running.

Record:

- runtime/API used;
- CPU usage;
- GPU usage where practical;
- overlay behavior;
- positioning behavior;
- application lifecycle;
- errors;
- limitations.

---

## 0.4 Steam Frame Standalone Test

Compile/deploy the experiment for Steam Frame ARM64 if the available SDK/API permits it.

Test:

```text
Steam Frame

Standalone VR Game
        +
HELLO FRAME overlay
```

Determine whether the overlay can remain visible while another standalone VR application is active.

### Success

If this works:

```text
STEAM FRAME STANDALONE OVERLAY = CONFIRMED
```

Document the exact API and deployment process.

This becomes the preferred long-term architecture.

### Partial Success

Examples:

- overlay works only with streamed PC VR;
- overlay works in SteamVR UI but disappears in standalone games;
- overlay requires special launch behavior;
- overlay works but input does not;
- overlay has compositor restrictions.

Document exactly what works.

Do NOT hide limitations behind abstractions.

### Failure

If persistent standalone overlays are not available:

Do not abandon the project.

Document the limitation and continue with the PC-hosted implementation.

Keep the architecture portable because Valve may expose the capability later.

---

# Sprint 1 — Project Foundation

**Status:** `[x] Complete — animated texture visually confirmed on Steam Frame`

- `[x]` Portable Core, VR overlay, and receiver application boundaries
- `[x]` Structured console logging and validated command-line configuration
- `[x]` Networking, decoder, video-source, and input contracts
- `[x]` Generated animated RGBA test source
- `[x]` OpenVR renderer with continuous updates and clean lifecycle handling
- `[x]` Windows x64 clean build and automated tests
- `[x]` Windows SteamVR runtime accepted overlay creation and 31 frame submissions in a timed run
- `[x]` Linux ARM64 cross-build produced an AArch64 receiver executable
- `[x]` Architecture and development documentation
- `[x]` User-visible confirmation that the generated animation updates in-headset
- `[!]` Known limitation: repeated `SetOverlayRaw` updates visibly flicker; replace with a streaming-appropriate texture path in Sprint 4/performance work

## Objective

Build the reusable application architecture.

Implement:

- core library;
- logging;
- configuration;
- basic networking abstractions;
- overlay renderer abstraction;
- video decoder abstraction;
- input abstraction;
- Windows platform implementation;
- Steam Frame placeholder/backend where appropriate.

Create a simple desktop/console test application capable of starting the VR overlay.

At the end of Sprint 1:

```text
PhoneCast Receiver
       │
       ▼
Generated Test Texture
       │
       ▼
IOverlayRenderer
       │
       ▼
SteamVR Overlay
```

Use an animated/generated test texture so continuous texture updates can be verified.

### Acceptance Criteria

- Project builds from a clean checkout.
- Development setup is documented.
- Overlay can start and stop cleanly.
- Texture can update continuously.
- Core does not depend directly on Win32.
- Windows-specific code is isolated.
- Architecture permits an ARM64 implementation.

---

# Sprint 2 — Android Screen Capture

**Status:** `[x] Complete — physical-device capture and encoding validated`

- `[x]` Android sender project and reproducible Gradle wrapper
- `[x]` MediaProjection permission flow
- `[x]` Android 14+ media-projection foreground-service lifecycle
- `[x]` Surface-input H.264/AVC MediaCodec pipeline at 30 FPS
- `[x]` Continuous encoder-output draining and frame/byte diagnostics
- `[x]` Start, stop, projection-revocation, and resource cleanup paths
- `[x]` Existing-VirtualDisplay resize with encoder-surface replacement
- `[x]` Debug APK build, JVM tests, and Android lint
- `[x]` Permission flow and captured frames confirmed on a physical Android device
- `[x]` Increasing H.264 frame/output counters confirmed on a physical Android device
- `[x]` Start/stop/restart and portrait/landscape rotation confirmed on a physical Android device

## Objective

Create the Android sender application.

Use Android's supported screen capture APIs, such as MediaProjection where appropriate.

Basic flow:

```text
Launch PhoneCast

      ↓

Select / Pair Receiver

      ↓

Android Screen Capture Permission

      ↓

Start Casting
```

The application should capture the Android display and encode it into a hardware-accelerated video stream when possible.

Initial target:

```text
Resolution:
phone native or sensible downscaled resolution

FPS:
30

Codec:
H.264

Network:
local network
```

Do not optimize prematurely for maximum image quality.

Prioritize latency and reliability.

### Acceptance Criteria

- User can grant screen capture permission.
- Screen frames are captured.
- Frames can be encoded.
- Stream can be started/stopped.
- App survives screen orientation changes reasonably.
- No VR integration required yet.

---

# Sprint 3 — Phone → PC Streaming

**Status:** `[x] Complete — responsive Android-to-PC streaming, reconnection, and rotation physically validated`

- `[x]` Transport alternatives researched and decision documented
- `[x]` Versioned, bounded binary framing protocol
- `[x]` Manual receiver connection and six-digit pairing gate
- `[x]` Android H.264 access-unit streaming with bounded queue
- `[x]` Automatic reconnect and keyframe request
- `[x]` Codec configuration and orientation-change propagation
- `[x]` Windows TCP receiver and Media Foundation H.264 decoder
- `[x]` Aspect-preserving desktop preview and basic diagnostics
- `[x]` C++ protocol tests, Windows build/tests, Android tests, APK, and lint
- `[x]` Physical Android-to-PC picture validation
- `[x]` Smooth LAN streaming physically validated with dynamic content, including YouTube playback in VR
- `[x]` Receiver restart/reconnection validation
- `[x]` Orientation validation — portrait-to-landscape and landscape-to-portrait transitions work end to end
- `[x]` Perceived latency physically accepted after enabling Media Foundation low-latency mode; touch-to-display was reported as near-instant and under one second, including isolated updates on static content (quantitative glass-to-glass measurement remains pending)
- `[x]` Right-edge green bar fixed by separating coded and visible decoder dimensions; physically confirmed in Sprint 4
- `[!]` Security limitation: pairing gates the stream, but Sprint 3 transport is not encrypted

## Objective

Connect the Android sender to the receiver.

Pipeline:

```text
Android
   │
Screen Capture
   ↓
H.264 Encoder
   ↓
Network Transport
   ↓
PC Receiver
   ↓
H.264 Decoder
   ↓
Video Frame
```

Initially prioritize LAN/Wi-Fi.

Investigate suitable transports.

Potential options include:

- UDP/custom transport;
- RTP;
- WebRTC;
- another low-latency protocol.

Do not automatically choose WebRTC merely because it is common.

Evaluate:

- latency;
- implementation complexity;
- Android support;
- Linux ARM64 support;
- packet loss handling;
- dependency size;
- future Steam Frame support.

Document the decision in:

```text
docs/protocol.md
```

### Acceptance Criteria

- Android discovers or connects to PC.
- Pairing is simple.
- Phone screen appears on PC.
- Stream maintains approximately 30 FPS under normal LAN conditions.
- Reconnection is handled.
- Orientation changes are handled.
- Latency measurements are available.

---

# Sprint 4 — Phone Screen in VR

**Status:** `[x] Complete — responsive visual MVP approved over a VR game in portrait and landscape`

- `[x]` Shared paired TCP video server used by desktop and VR receivers
- `[x]` Windows Media Foundation decoder connected to the OpenVR overlay
- `[x]` Reusable D3D11 texture submission through `SetOverlayTexture` on Windows
- `[x]` Aspect-preserving dimension updates physically validated in portrait and landscape; overall perceived scale remains consistent across rotation
- `[x]` Global show/hide, scale, distance, opacity, move, and reset controls
- `[x]` Decoder visible-versus-coded dimension handling removes the green edge
- `[x]` Windows clean build, automated tests, and generated-texture OpenVR runtime test
- `[x]` Physical Android stream visible in the SteamVR headset
- `[x]` D3D11 overlay uses SteamVR's DXGI adapter and explicitly flushes updates; generated animation is visually smooth
- `[x]` Phone overlay remains visible over a running VR game
- `[x]` Physical validation of PC keyboard overlay controls
- `[x]` Orientation validation: repeated portrait/landscape transitions work and retain comparable physical scale
- `[x]` Physical validation confirms the right-edge green bar is fixed
- `[x]` Dynamic and static content are responsive after the decoder low-latency fix; YouTube playback is watchable and no multi-second stalls were reported in the latest physical approval test
- `[x]` Initial overlay appearance physically reported as near-instant after the decoder low-latency fix
- `[~]` Latency physically accepted as near-instant and under one second from touch to display; record quantitative sustained FPS and glass-to-glass measurements during the later performance pass
- `[x]` Overlay lifecycle ignores unrelated `VREvent_ProcessQuit` events during VR scene transitions
- `[!]` Test-environment hazard: starting SteamVR under Windows RDP can break VRLink D3D11 texture creation and produce a gray stream. Automated console transfer and startup are physically observed, but VRLink still freezes before a later logged RDP reconnection; sustained recovery is not validated. SteamVR-only and visible/head-locked PhoneCast diagnostic launchers with session monitoring are ready for A/B testing (see `docs/development.md` and `docs/rdp-vrlink-recovery.md`).

## Session Handoff Snapshot — 2026-10-02

Latest commits:

- `dab5fc2` — rolling performance diagnostics and keyframe-safe recovery;
- `9c961b0` — encoder cadence cap and immediate waiting overlay;
- `763d84a` — static-screen experiment and larger startup queue;
- `5a2323c` — 1280-pixel sender, codec latency settings, and cached startup keyframe.

Physically confirmed on Windows PC → VRLink → Steam Frame:

- Android portrait screen appears as a head-relative OpenVR overlay and remains visible over a running VR game;
- PC keyboard overlay controls work, including before phone video arrives because a waiting texture is submitted at startup;
- generated D3D11 animation is smooth after selecting OpenVR's DXGI adapter and flushing updates;
- the right-edge green bar is fixed;
- dynamic phone content can sustain approximately 30 received/decoded FPS with no receiver drops;
- the latest 590 × 1280 stream reduced decode time from roughly 5–6 ms to roughly 2.5–3 ms and reduced typical bitrate to roughly 1–2.6 Mbps.

Historical blockers at this handoff (subsequently resolved unless noted):

1. The approximately 2–5+ second action-to-visible latency was resolved by enabling Media Foundation low-latency mode.
2. The apparent multi-second sender stalls were sparse static-screen updates held by decoder look-ahead, not a demonstrated sender failure.
3. First-frame startup is now physically reported as near-instant; quantitative measurement remains deferred to Sprint 10.
4. Landscape reconfiguration and consistent rotation scale are physically validated.
5. Quantitative glass-to-glass latency remains deferred to Sprint 10.

Most important evidence:

- The receiver is not accumulating a delayed queue. In the latest run, queue age was generally about 5–15 ms, Media Foundation decode about 2.5–3 ms at 590 × 1280, D3D/OpenVR submission about 0.2–0.3 ms, queue depth zero, and receiver drops/resyncs zero during steady-state streaming.
- During visible lag, diagnostics repeatedly report `rx-fps=0` for many consecutive seconds while the TCP connection remains established. This establishes sparse incoming video, but does **not** by itself prove a sender stall: static-screen capture can legitimately stop producing frames while the decoder retains the last update internally. Android capture/composition, MediaCodec output, the sender queue, and blocked TCP writes remain possible additional causes.
- When Android does deliver motion, the complete downstream path sustains approximately 30 FPS and renders roughly 25–30 FPS. Fast decode calls and an empty receive queue do not measure buffering inside Media Foundation; the follow-up below reproduces that missing source of latency.
- `KEY_MAX_FPS_TO_ENCODER=30` fixed the original 90–120 FPS flood. `KEY_REPEAT_PREVIOUS_FRAME_AFTER` did not provide a reliable heartbeat on the test codec and was removed.
- The latest sender uses a 1280-pixel long edge, two-second keyframe interval, CBR when supported, the advertised low-latency codec feature when supported, no B-frames, and a cached keyframe for reconnect. These reduced bandwidth and decode cost but did not eliminate the upstream stalls.
- Latest startup example: config at approximately 343 ms, decoder initialization 14 ms, first keyframe at 374 ms, but first decoded/submitted frame at approximately 4.93 seconds. An earlier run waited approximately 16.5 seconds for the first keyframe.
- The current Windows path still performs NV12 → CPU RGBA → D3D11 upload, but measured cost is far below the observed latency.
- Do not test through RDP; confirm `query session` shows the user on `console` before launching SteamVR.
- At handoff time, the VR receiver was running on TCP port `49321` with pairing code `123456`; process IDs are ephemeral and must be rechecked.

### Decoder latency follow-up — 2026-10-02

- `[x]` Reproduced hidden Media Foundation buffering with a generated H.264 regression fixture: the initial IDR produced no output until future input with the old configuration.
- `[x]` Enabled `MF_LOW_LATENCY` (the `CODECAPI_AVLowLatencyMode` GUID, UINT32) before decoder media-type negotiation. Failure to enable it is reported rather than silently accepting buffering.
- `[x]` The same test now produces the IDR and each subsequent P-frame immediately, without future input or an end-of-stream drain. This affects both desktop and VR receivers; no Android APK change is needed.
- `[x]` Full Windows build and all nine CTest tests pass in `out/build/windows-x64-latency`. The original build's VR executable was locked by the running receiver, which was left undisturbed.
- `[x]` Restarted the fixed VR receiver in the physical console session; the user retested and reported “this is perfect. very snappy.” This confirms a substantial perceived responsiveness improvement in the phone-to-VR path.
- `[x]` Follow-up VR validation covered startup, isolated/static updates, and sustained dynamic content. The user reported near-instant startup, touch-to-display latency under one second and near-instant even for static content, and watchable YouTube playback, then explicitly approved the result.
- `[ ]` Quantitatively measure startup and glass-to-glass latency during the performance pass; current timing is a user estimate rather than an instrumented measurement.
- `[x]` Repeated portrait/landscape transitions physically validated; landscape is responsive and its overall scale matches portrait.

The prior inference that decoder buffering was ruled out was incorrect. Milliseconds spent inside `Submit` measure work, not how long a picture waits for future input. Sparse screen updates can turn a small decoder look-ahead into seconds of visible delay. The reproduced fix is now user-approved across static and dynamic content; replacing or further instrumenting Android's capture path is not justified without new evidence of stalls.

### Sprint 4 completion validation — 2026-10-02

- The user physically approved responsive static and dynamic content, including watchable YouTube playback.
- Startup was reported as near-instant and touch-to-display response as under one second; instrumented measurement remains Sprint 10 work.
- Portrait-to-landscape and landscape-to-portrait transitions work repeatedly.
- Landscape width is adjusted so the phone retains approximately the same overall/diagonal scale as portrait.
- Locking the Android device may show black briefly and ends or invalidates screen projection. On Android 15 QPR1 and newer, Android explicitly stops MediaProjection when the screen locks as a privacy measure. A stopped projection cannot be resumed; after unlocking, the user must start a new capture session and grant consent as required. PhoneCast must not attempt to bypass lock-screen capture restrictions.
- Primary Android references: [Media projection — status bar chip and auto stop](https://developer.android.com/media/grow/media-projection#status_bar_chip_auto_stop), [Android 15 behavior changes](https://developer.android.com/about/versions/15/behavior-changes-all#media-projection-status-bar-chip), and [`MediaProjection.Callback`](https://developer.android.com/reference/android/media/projection/MediaProjection.Callback).

Recommended next work:

1. Begin Sprint 5 with a small placement-model design covering world-, head-, left-controller-, and right-controller-locked modes.
2. Implement and test world/head placement switching before controller attachment or grab interaction.
3. Preserve the existing head-relative behavior as the default and add settings persistence.
4. Leave instrumented startup, sustained FPS, and glass-to-glass latency for Sprint 10 unless performance regresses.

## Objective

Connect the video decoder output to the VR overlay.

Pipeline:

```text
ANDROID
   │
   ▼
H.264
   │
   ▼
NETWORK
   │
   ▼
DECODER
   │
   ▼
GPU TEXTURE
   │
   ▼
OPENVR OVERLAY
   │
   ▼
VR
```

Avoid unnecessary GPU → CPU → GPU frame copies where practical.

The overlay should initially be simple.

Controls:

- Show
- Hide
- Scale
- Distance
- Opacity
- Reset position

Default appearance should preserve the phone's aspect ratio.

### Acceptance Criteria

The user can:

1. Launch SteamVR.
2. Launch PhoneCast.
3. Start a VR game.
4. Start casting from Android.
5. See the phone screen over the VR game.
6. Resize it.
7. Move/reset it.
8. Hide/show it.

At this point the project has reached its **first true MVP**.

---

# Sprint 5 — VR Placement System

**Status:** `[x] Complete for the PC-hosted path — physically approved over a PC SteamVR game`

- `[x]` Portable head/world/left-controller/right-controller placement model
- `[x]` OpenVR tracked-device-relative and standing-space transforms
- `[x]` World anchor creation at the current head-relative location
- `[x]` Keyboard placement-mode switching and mode-aware adjustment
- `[x]` Trigger-and-point direct VR grab/release placement path
- `[x]` Versioned settings persistence with safe defaults and validation
- `[x]` Windows clean build and all automated tests
- `[x]` Physical validation that placement controls and controller-facing behavior function
- `[!]` Controller-locked placement is functional but does not yet match the desired ergonomic pose; calibration is deferred to Sprint 5.1
- `[x]` In-VR placement behavior validated while a PC SteamVR game runs
- `[!]` The Windows-hosted overlay disappears in a headset-native standalone game; this is expected for the current PC compositor path and remains Sprint 11/native-backend work

## Objective

Make the overlay feel native to VR.

Implement positioning modes.

## World Locked

The phone stays at a location in VR space.

```text
ROOM

         PHONE
       ┌───────┐
       │       │
       └───────┘

              USER
```

## Head Locked

Phone follows the user's view.

Allow configurable offsets.

Example:

```text
          VIEW

              ┌───────┐
              │ PHONE │
              └───────┘
```

Prefer peripheral positioning rather than blocking the center of vision.

## Wrist / Controller Locked

Attach the phone to a controller pose.

Example:

```text
      ┌────────────┐
      │   PHONE    │
      └────────────┘
            │
            │
         CONTROLLER
```

Allow left/right controller selection.

### Physical Validation Result — 2026-10-02

The user confirmed the Sprint 5 placement system works while playing a PC SteamVR game. When switching to a headset-native standalone game, the Windows-hosted overlay disappears. This does not block Sprint 5 because its implemented receiver targets the PC SteamVR compositor. It does not validate native-overlay coexistence with a standalone VR scene; that remains an explicit Steam Frame risk and Sprint 11 responsibility.

### Acceptance Criteria

User can switch between:

- World
- Head
- Left controller
- Right controller

Allow direct VR placement adjustment (for example, grab/move or equivalent controller controls) so repositioning does not require the PC keyboard.

Settings persist between sessions.

---

# Sprint 5.1 — Controller Placement Calibration

**Status:** `[x] Complete for the implemented calibration scope — project owner closed Sprint 5.1; extended ergonomic tuning deferred to ongoing use`

- `[x]` Independent persisted left- and right-controller calibration profiles
- `[x]` Distance, height, lateral position, tilt, yaw, and scale adjustment
- `[x]` Controller-relative, face-user, world-upright, and wrist orientation modes
- `[x]` In-headset controller calibration controls with live adjustment and per-hand reset
- `[x]` Tracking-loss behavior retains the last valid overlay transform and resumes on recovery
- `[x]` Windows clean build and all automated tests
- `[x]` Explicit SteamVR Input and Steam Frame binding experiment completed; runtime accepted the assets, but the broad action set is now dormant and ordinary operation retains non-global compatibility polling
- `[!]` Physical retest: the phone overlay was visible and stable and keyboard controls worked, but global SteamVR Input commands were not delivered reliably while another application owned focus
- `[~]` Extended headset tuning of defaults, stability, and ergonomics is deferred to ongoing interaction time rather than blocking later functional sprints

## Objective

Make controller-locked placement configurable and comfortable rather than enforcing one orientation policy.

Implement and physically tune:

- independent left- and right-controller offsets;
- configurable distance, height, lateral position, tilt, yaw, and scale;
- selectable orientation behavior, including controller-relative, face-the-user, world-upright billboard, and a practical wrist-style pose;
- an in-headset calibration flow with live adjustment and reset;
- per-hand persisted calibration profiles;
- stable behavior while the controller rotates, moves, or temporarily loses tracking.

Do not guess a universal pose. The configurable implementation is accepted as the Sprint 5.1 deliverable. The current defaults remain provisional, and later interaction time may produce follow-up calibration changes without reopening this sprint. The OpenVR dashboard is the supported route into these controls; experimental global bindings are not an acceptance dependency.

### Closure decision — 2026-10-03

The project owner closed Sprint 5.1 so functional work can continue. This records acceptance of the implemented model and automated coverage, not a claim that every controller pose or ergonomic default has been physically validated. Remaining manual tuning is deferred and should be captured as follow-up evidence during later headset use.

### Acceptance Criteria

- User can configure a comfortable pose for either hand without editing files or using a PC keyboard.
- Left and right controller settings persist independently.
- At least controller-relative and face-the-user orientation modes are available.
- The overlay does not visibly flip, roll, or jitter during ordinary controller movement.
- Default values are safe and adjustable; final ergonomic approval is explicitly deferred by the closure decision above.

---

# Sprint 6 — Glance Mode

**Status:** `[~] Functional baseline accepted for progression — dashboard controls physically approved; extended manual validation deferred while Sprint 7 begins`

- `[x]` Portable Hidden, Glance, Expanded, and Pinned state model
- `[x]` Small initiating-hand controller preview without overwriting persisted placement
- `[x]` Keyboard cycle and quick show/hide fallback
- `[x]` Hidden startup with decoding retained for immediate reveal
- `[x]` Phone-shaped Settings panel with laser-selectable rows and `-`/`+` adjustment targets
- `[~]` Menu flicker mitigation removes hover-driven raw-texture redraws; physical confirmation pending
- `[x]` Automated state and presentation tests
- `[x]` Keyboard-driven state transitions and live, quick reveal physically confirmed
- `[x]` Keyboard left/right placement selection selects the hand used by the next keyboard-driven Glance preview
- `[x]` OpenVR dashboard tab/icon with Show/Hide, Glance, Pin, Settings, and placement controls
- `[x]` Dashboard icon, panel, and controller-laser controls physically validated and approved
- `[x]` OpenVR dashboard adopted as the primary in-headset launcher and recovery path
- `[!]` SteamVR Input actions did not reliably deliver controller commands while another application owned focus; overlay-global overrides are not required by the chosen dashboard path
- `[x]` Wrist-pose gesture, progress indicator, and thumbstick long-press control grid disabled at runtime; dormant experimental code is retained only for possible future reconsideration
- `[x]` Input-safety follow-up complete: ordinary dashboard-only operation no longer initializes or submits the broad overlay-global action set, so disabled gesture handlers cannot claim bound game-controller sources
- `[x]` Controller-only settings navigation, live editing, explicit apply/cancel, and intentional reset without requiring the PC keyboard
- `[x]` Settings changed in-headset persist through the portable settings model only when applied
- `[ ]` Native ARM64 dashboard lifecycle and operation over a standalone VR scene remain to be physically validated
- `[ ]` Physical approval of the 55% Glance preview size and per-hand offsets remains pending

## Objective

Prevent the phone from constantly cluttering the user's VR view while providing a dependable, supported way to recover it when hidden.

Retain the portable presentation states:

```text
Hidden
   ↓
Glance
   ↓
Expanded
   ↓
Pinned
```

For the current product direction, the **OpenVR dashboard tab is the sole primary in-headset control surface**. The user opens the normal SteamVR dashboard, selects PhoneCast, and chooses Show/Hide, Glance, Pin, Settings, or placement controls with the controller laser.

The wrist-pose gesture is disabled for now. Do not require experimental overlay-global SteamVR Input overrides, thumbstick shortcuts, or gesture calibration for ordinary use. The dormant gesture work may be revisited only if a future headset test establishes a clear need beyond the dashboard path.

Disabling gesture handlers alone is not sufficient input isolation because OpenVR states that higher-priority bindings can disable lower-priority bindings using the same source. The OpenVR backend therefore no longer initializes or submits its broad overlay-global action set during ordinary dashboard-only use. The dormant action manifest and implementation may remain for controlled future research, but PhoneCast does not call `UpdateActionState` for that set in the supported configuration.

Sprint 6 no longer blocks Sprint 7. Because this sprint is primarily UI and interaction work, its remaining manual validation will be accumulated during normal use while later functional interaction is implemented. Defects found during that use should be fixed as focused follow-up work; untested lifecycle or ergonomic behavior must remain recorded as unvalidated.

## In-Headset Settings

Expand Sprint 6 beyond reveal controls with a PhoneCast settings section opened from the dashboard's **Settings** control. The user should not need the PC keyboard, experimental global input overrides, gestures, or configuration-file editing for ordinary overlay setup.

The first settings surface should cover functionality that already exists:

- overlay visibility, scale, opacity, distance, and reset;
- head-, world-, left-controller-, and right-controller placement;
- independent left/right controller calibration and orientation behavior from Sprint 5.1;
- Glance preview scale; keep the dormant radial-menu long-press setting out of the active UI unless experimental shortcuts are intentionally restored;
- concise connection and active-backend status where useful.

Use a separate in-headset panel or menu layer rather than drawing settings into the streamed phone image. Keep its settings/state model platform-independent so the same UI behavior can be reused by the Steam Frame native backend. Platform renderers may implement the actual compositor surface.

Do not pull future performance presets, Android remote-control permissions, notifications, or standalone lifecycle settings into Sprint 6 before their owning sprints establish those models. The settings section should be extensible so those categories can be added later.

### Acceptance Criteria

- The normal SteamVR dashboard exposes a recognizable PhoneCast tab while the receiver runs.
- Show/Hide recovers the phone from fully Hidden without a custom global binding or wrist gesture.
- Dashboard Glance, Pin, Settings, and placement controls respond reliably to the controller laser.
- Closing the dashboard releases input immediately and does not activate the underlying game.
- Settings can be opened, navigated, changed, applied/cancelled, and closed using VR controllers.
- Existing appearance, placement, controller-calibration, and Glance settings are available without a PC keyboard.
- Changes render live where safe and persist across receiver restarts only when applied.
- Cancel restores the pre-edit values; reset requires an intentional action.
- The panel is readable and does not unnecessarily obstruct central gameplay view.
- The wrist-pose gesture and its progress indicator do not activate during ordinary use.
- No overlay-global action set claims controller buttons, axes, grips, or triggers during ordinary dashboard-only use.
- Native ARM64 dashboard lifecycle and standalone-VR-scene coexistence are recorded separately until physically tested.

## Overlay shortcut research decision — 2026-10-03

Research into [`KominoVR/frame-passthrough-shortcuts`](docs/frame-passthrough-shortcuts-research.md) establishes a useful but optional future path:

- a native ARM64 `VRApplication_Overlay` can receive SteamVR Input shortcuts without rendering an overlay surface;
- the demonstrated implementation uses `k_nActionSetOverlayGlobalPriorityMax` and narrow, user-remappable thumbstick-button bindings;
- SteamVR's binding layer can recognize double-press and long-press gestures without application-side timing code;
- even maximum public overlay-global priority does not override SteamVR dashboard input focus, so those shortcuts work only while the dashboard is closed;
- action activity must be diagnosed with per-action `bActive`, binding-load failures, dashboard visibility, and backend/runtime information rather than manifest-load success alone.

This does not change the current dashboard-first direction. If optional hotkeys are revisited, place them in a separate narrow action set, keep them opt-in, deactivate them while the dashboard is visible, avoid binding axes/grips/triggers unnecessarily, and test native Steam Frame separately from Windows/VRLink. Do not reactivate the existing broad action set as a shortcut.

---

# Sprint 7 — VR Interaction

**Status:** `[x] Complete for the PC-hosted path — controller interaction and game-input coexistence physically approved`

- `[x]` Portable normalized pointer event model and bounded wire encoding
- `[x]` OpenVR overlay mouse/raycast coordinates mapped to top-left phone UV coordinates
- `[x]` Trigger tap, hold/drag/release, and runtime scroll event capture
- `[x]` Dashboard and lower-left overlay Android Back actions
- `[x]` Dedicated horizontal grab handle below the phone avoids grip-button conflicts and preserves ordinary trigger interaction
- `[x]` Continued Android accessibility strokes support live held-trigger drag scrolling
- `[x]` Automated coordinate, handle/back-button inset, orientation, sequence, and protocol tests
- `[x]` Physical tap, long-press, four-direction swipe, runtime scroll, live held-trigger drag scrolling, and corner-coordinate validation in portrait and landscape
- `[x]` Dashboard and lower-left Back controls physically validated
- `[x]` Bottom-handle placement and stable world-lock release physically validated in portrait and landscape
- `[x]` Reconnect plus both active-stream opt-out/re-enable gates physically validated
- `[x]` Running-game regression fixed by removing persistent `MakeOverlaysInteractiveIfVisible`; the visible phone keeps updating while the dashboard is closed and game hand input remains active
- `[x]` Dashboard-open phone interaction works, and closing the dashboard immediately restores priority to the game
- `[~]` Controller/hand-locked placement can visibly jitter while the overlay is swept across the view; accepted as a non-blocking follow-up rather than characterized as eliminated

## Objective

Allow interaction with the displayed phone.

Implement VR pointer/raycast interaction. This sprint also provides the headset-native interaction path; Sprint 4's PC keyboard shortcuts control only the overlay's visibility/placement/appearance and do not interact with the displayed phone.

```text
Controller
     \
      \
       \ ray
        \
      ┌──────────────┐
      │              │
      │      •       │
      │              │
      └──────────────┘
```

Convert VR overlay coordinates into normalized UV coordinates:

```text
VR Hit
   ↓
Overlay UV
   ↓
Phone Resolution
   ↓
Phone X/Y
```

Define a platform-independent input protocol.

Potential events:

```text
TOUCH_DOWN
TOUCH_MOVE
TOUCH_UP
SCROLL
BACK
HOME
TEXT_INPUT
```

The receiver should not care how Android eventually implements these actions.

---

# Sprint 8 — Android Remote Control

**Status:** `[x] Complete for the PC-hosted path — Android permissions, gestures, lifecycle, and opt-out physically approved`

- `[x]` Accessibility, ADB, scrcpy-style, device-owner, privileged, and root alternatives investigated
- `[x]` Optional user-enabled AccessibilityService selected as the public non-root path
- `[x]` Separate in-app consent toggle, prominent disclosure, and Android Settings enablement flow
- `[x]` Accessibility service does not retrieve window content or inspect app text
- `[x]` Tap/long-press, swipe, scroll, and Android Back implementation
- `[x]` Remote input accepted only over the active phone-initiated streaming connection after its pairing handshake
- `[x]` Android JVM tests, debug APK, and lint
- `[!]` Existing transport remains unencrypted; use only on a trusted LAN
- `[!]` Accessibility use has Google Play policy/declaration implications documented in `docs/remote-control.md`
- `[x]` Physical phone validation of permissions, gestures, portrait/landscape coordinates, reconnect, and opt-out
- `[x]` Project owner approved Sprints 7 and 8 after running-game coexistence validation

## Objective

Determine the safest and most practical method for sending interaction back to Android.

Investigate:

- Android Accessibility APIs;
- ADB;
- scrcpy-related techniques;
- developer-mode solutions;
- device-owner capabilities;
- standard Android restrictions.

The implementation should clearly document which permissions are required.

Do not silently depend on root.

Root should NOT be required for the normal product unless no viable alternative exists.

Possible interaction mapping:

```text
VR Trigger
      ↓
Tap

Trigger Hold
      ↓
Long Press

Controller Drag
      ↓
Swipe

Thumbstick
      ↓
Scroll

VR Keyboard
      ↓
Text Input
```

### Acceptance Criteria

At minimum:

- tap;
- swipe;
- scroll;
- Android Back action

should function from VR if supported by the selected Android integration.

---

# Sprint 9 — Notification Bridge

**Status:** `[x] Complete for the PC-hosted path — notification display, placement, and open-phone action physically approved`

- `[x]` Optional Android NotificationListenerService with explicit OS access
- `[x]` Forwarding disabled by default with a separate sensitive-content opt-in
- `[x]` Package allowlist and blocklist filtering
- `[x]` Versioned, bounded notification wire payload on the active paired stream
- `[x]` Notification traffic isolated from H.264 queue and keyframe recovery state
- `[x]` Portable notification renderer contract and transient OpenVR card
- `[x]` Six-second peripheral card with newer-notification replacement
- `[x]` Persisted in-headset notification size, distance, horizontal, and vertical settings with safer defaults
- `[x]` Dashboard-laser card selection opens the full phone overlay
- `[x]` C++ protocol tests, Windows build/tests, Android tests, APK, and lint
- `[x]` Physical confirmation that cards appear with the dashboard open or closed and selecting a card opens the phone
- `[x]` Revised default placement is readable and approved after the original fixed upper-right placement was rejected
- `[x]` Project owner accepted Sprint 9 for progression
- `[~]` Allow/block filtering, redaction variants, access revocation, and game-input coexistence retain automated/design coverage but were not each physically exercised before closure
- `[ ]` Native ARM64 notification-card lifecycle remains Sprint 11 validation

## Session Handoff Snapshot — 2026-10-04

Sprint 9 is closed for the Windows PC-hosted path by project-owner approval.

Implemented:

- Android notification forwarding is separately opt-in and requires Android notification-listener access;
- notification title/body content is redacted by default and requires a separate sensitive-content opt-in;
- comma-separated package allowlists and blocklists are applied on Android before serialization;
- notification payloads are versioned, bounded, validated, and carried only over the active paired stream;
- notification traffic uses a separate bounded sender queue and does not affect H.264 frame sequencing or keyframe recovery;
- OpenVR displays the newest card for six seconds independently of full-phone visibility;
- cards remain visible with the SteamVR dashboard open or closed;
- selecting a card with the dashboard laser opens the full phone overlay;
- Settings → Notifications provides live Size, Distance, Horizontal, and Vertical controls with transactional apply/cancel and version-3 persistence;
- the accepted defaults are 0.55 m width, 0.75 m distance, +0.18 m horizontal offset, and +0.12 m vertical offset;
- the root launcher now uses the canonical `out/build/windows-x64` receiver rather than the stale pre-Sprint-9 review build.

Validation:

- the project owner physically confirmed card visibility, dashboard-open/dashboard-closed behavior, open-phone selection, and the revised default placement;
- all 11 Windows CTest tests pass;
- Android JVM tests, debug APK assembly, and lint pass;
- Docker cross-build produces an AArch64 receiver executable;
- receiver logs confirm authenticated streaming, displayed notification cards, and a card-triggered full-phone request.

Known limitations and deferred evidence:

- the paired TCP transport remains unencrypted, so notification forwarding is restricted to a trusted LAN;
- allow/block combinations, redacted-versus-content cards, notification-access revocation, and running-game input coexistence were not each manually exercised before owner-approved closure; do not represent those individual cases as physically tested;
- native Steam Frame ARM64 notification rendering and lifecycle remain Sprint 11 work.

Recommended next work:

1. Begin Sprint 10 with instrumented startup, sustained FPS, glass-to-glass latency, CPU/GPU, memory, bandwidth, and VR-frametime measurements.
2. Preserve notification/video queue isolation while measuring load.
3. Do not fold native Steam Frame backend implementation into Sprint 10; it remains Sprint 11.

## Objective

Make PhoneCast useful without displaying the entire phone.

Android companion app should optionally forward notifications.

Instead of:

```text
FULL PHONE
```

display:

```text
┌──────────────────────────────┐
│ 💬 Messages                 │
│                              │
│ Are you still coming over?   │
│                        9:42  │
└──────────────────────────────┘
```

Notifications should:

- appear briefly;
- avoid obstructing central vision;
- respect an application allowlist/blocklist;
- support disabling notification forwarding entirely;
- optionally open the full phone overlay.

Privacy is important.

Do not expose sensitive notification content unless explicitly enabled.

---

## Post-Sprint UI and control feedback pass — complete and project-owner approved

- `[x]` Headset settings terminology now matches Left Dock and Right Dock controls.
- `[x]` Numeric headset settings provide interactive sliders while retaining precise `-` / `+` controls.
- `[x]` The settings panel opens world-locked and has an independent controller-drag move handle.
- `[x]` Settings and notification compositor surfaces use styled cards, visual hierarchy, and distinct actions rather than plain text blocks.
- `[x]` Entering headset Notification settings shows a live representative card for placement and appearance preview; leaving the category removes it.
- `[x]` Selecting a notification reveals the phone and invokes the original Android notification launch action when available.
- `[x]` The phone footer includes a small lower-right close control alongside Android Back.
- `[x]` Android launch UI is reduced to saved receiver details, live state, and one Start/Stop action; optional controls are behind a menu.
- `[x]` Android refreshes stale casting controls after MediaProjection ends externally, including screen-lock revocation.
- `[x]` Android reports both remote-control gates independently; the receiver console and dashboard warn when either is closed.
- `[x]` Windows clean build/tests and Android JVM tests/APK/lint pass.
- `[x]` Project owner physically approved the revised Android/headset experience, notification preview and placement, notification-to-app navigation, local close control, and feedback-round behavior on 2026-10-04.

---

# Sprint 10 — Performance Pass

**Status:** `[x] Complete — Standard profile performance physically measured and project-owner approved`

- `[x]` Flushed one-second CSV logging for startup, sustained FPS, bitrate, decode/render cost, queue age/depth, drops, and resyncs
- `[x]` Windows receiver process CPU, working-set, and private-memory sampling
- `[x]` Portable OpenVR compositor frame-timing snapshot including GPU/CPU time, dropped/mispresented frames, and reprojection flags
- `[x]` Persisted Android Battery Saver, Standard, and Quality encoder profiles
- `[x]` Repeatable no-PhoneCast/profile comparison and external glass-to-glass measurement procedure
- `[x]` Standard profile physically measured over a roughly 253-second connected run and accepted by the project owner
- `[x]` Instrumented Standard startup: configuration 340.6 ms, first keyframe 372.2 ms, decoder initialization 4.25 ms, and first submitted frame 381.8 ms
- `[x]` Dynamic Standard averages: 29.93 receive/decode FPS, 28.40 render FPS, 2.00 Mbps, 3.78 ms decode, 0.37 ms submission, and 8.83 ms queue age
- `[x]` Dynamic Standard resource evidence: 1.13% process CPU average, approximately 69 MiB working set, and zero OpenVR-reported dropped or mispresented VR frames
- `[~]` Battery Saver and Quality remain available measurement candidates; project-owner approval did not require separate physical characterization before closure
- `[~]` External-camera glass-to-glass distributions remain deferred; the approved run measured first submission rather than photon-level latency
- `[~]` Occasional render coalescing dips were logged without transport drops; no user-visible defect was reported as part of approval
- `[!]` Direct decoder-to-GPU texture sharing was not justified by the measured CPU/decode/submission cost and remains possible future optimization

### Sprint 10 closure evidence — 2026-10-04

The project owner approved Sprint 10 after a physical Standard-profile headset run. The receiver used a 590 × 1280 stream for approximately 253 connected seconds. During dynamic periods it sustained 29.93 received and decoded FPS, averaged 28.40 submitted updates per second, approximately 2.00 Mbps, 3.78 ms decode time, 0.37 ms D3D/OpenVR submission time, and 8.83 ms queue age. Dynamic transport drops and resynchronizations were zero. The process averaged 1.13% host CPU and approximately 69 MiB working set; OpenVR reported no dropped or mispresented VR frames.

The connection reached first submitted video in 381.8 ms. This is not a photon-level glass-to-glass measurement. The run also does not separately approve Battery Saver or Quality. Those limitations are explicitly accepted for progression and must remain documented rather than being described as tested.

## Objective

Minimize impact on VR gaming.

Measure:

- time from sender connection/start-cast to first visible overlay frame;
- codec-configuration and initial-keyframe wait;
- CPU usage;
- GPU usage;
- decoder utilization;
- network bandwidth;
- frame latency;
- dropped frames;
- memory;
- VR frametime impact.

Target architecture:

```text
Android Hardware Encoder
          ↓
      Network
          ↓
Hardware Decoder
          ↓
     GPU Texture
          ↓
VR Compositor
```

Avoid:

```text
GPU
 ↓
CPU copy
 ↓
CPU conversion
 ↓
GPU upload
```

where reasonable.

Allow configurable streaming modes:

```text
Battery Saver
Standard
Quality
```

Example starting targets:

```text
Battery Saver
720p / 20 FPS

Standard
1080p-ish / 30 FPS

Quality
higher resolution / 30-60 FPS
```

Actual presets should be based on measurements rather than these placeholder values.

Reduce initial appearance delay where measurements show avoidable receiver, decoder, keyframe-request, or texture-startup latency.

---

# Sprint 11 — Steam Frame Native Backend

**Status:** `[~] Reusable Vulkan renderer physically approved — remaining native lifecycle and VR-scene validation pending`

- `[x]` Shared paired TCP server moved out of the Windows platform boundary and built for Linux ARM64
- `[x]` Native stateful V4L2 M2M H.264 decoder with automatic device discovery and explicit device override
- `[x]` Native NV12/NV12M-to-RGBA frame path connected to the existing OpenVR overlay, dashboard, interaction, settings, and notification application
- `[x]` Linux signal shutdown, XDG settings persistence, and process CPU/memory diagnostics
- `[x]` Native streaming receiver manifest and ARM64 packaging target
- `[x]` Windows x64 build and all automated tests remain passing
- `[x]` Linux ARM64 cross-build produces an AArch64 `phonecast-vr-stream-receiver`
- `[x]` Deployed the streaming receiver to Steam Frame and confirmed Qualcomm `iris_driver`, `/dev/video-dec0`, H.264 input, and NV12 capture
- `[x]` Native Android video is visible and sustains approximately 30 received/decoded/rendered FPS in steady periods; the user reported functional controls
- `[~]` Dashboard, placement, and interaction are operational in initial use; detailed notifications, reconnect, sleep/wake, and lifecycle cases remain to be exercised
- `[x]` Native first-use width changed to the physically preferred 0.20 m without overwriting persisted settings
- `[x]` Reusable double-buffered Vulkan `SetOverlayTexture` path implemented behind the OpenVR platform boundary and Linux ARM64 cross-built
- `[x]` Vulkan path physically approved by the project owner on 2026-10-04 as smooth and flicker-free during phone interaction and a locally running Hades session; memory remained stable and no texture submission failure occurred
- `[~]` Rotation and clean shutdown of the Vulkan path remain separate physical checks
- `[ ]` Validate coexistence over a standalone VR scene application, including both launch orders

## Session Handoff Snapshot — 2026-10-04

Branch and commits:

- branch: `review/sprint-11-steam-frame-native`;
- `cf02bdc` — portable TCP server and native V4L2 streaming receiver;
- `d69429e` — Steam Frame bundle on the local Node download page;
- `27b4663` — native installation evidence;
- `95cef15` / `370705c` — corrected waiting-panel geometry and Qualcomm empty-event handling;
- `ebf53c4` — recorded physical validation and selected 0.20 m native default.

Physically confirmed on Steam Frame:

- Android authenticated directly to the headset and sent a 590 × 1280 H.264 stream;
- Qualcomm `iris_driver` at `/dev/video-dec0` decoded to NV12;
- live phone video became visible after treating both `EAGAIN` and Qualcomm's `ENOENT` as an empty V4L2 event queue;
- the user reported all exercised functions operational;
- the preferred overlay width is 0.20 m and is persisted on the headset;
- steady dynamic periods commonly reached approximately 30 receive/decode/render FPS, 1.7–2.4 Mbps, 4–5 ms decode, 1.5–1.8 ms render submission, about 2–2.5% sampled process CPU, no transport drops/resyncs, and no OpenVR-reported dropped frames.

Prior blocking result:

- repeated `SetOverlayRaw` calls visibly flickered;
- working set rose from roughly 88 MiB into the hundreds of MiB during sustained streaming;
- OpenVR eventually returned `VROverlayError_RequestFailed (23)` and the receiver exited;
- the installed raw-upload receiver is stopped and must not be represented as suitable for sustained use.

Renderer implementation update:

- Linux now loads `libvulkan.so.1` at runtime and requests OpenVR's required Vulkan instance/device extensions on the compositor GPU;
- one persistently mapped staging buffer feeds two reusable device-local RGBA images;
- images are synchronized with an upload fence and submitted through `SetOverlayTexture` as `TextureType_Vulkan`;
- stream-size changes clear the old compositor texture before recreating resources;
- graphics handles remain inside `platform/openvr`, while the V4L2 decoder and portable CPU `VideoFrame` remain unchanged;
- teardown clears the compositor texture, destroys overlays, calls `VR_Shutdown`, and then releases Vulkan resources;
- Windows x64 still uses its established reusable D3D11 path.
- The Vulkan build was installed on Steam Frame and started successfully: OpenVR selected `Turnip Adreno (TM) 750`, enabled five instance and eight device extensions, and created one reusable 590 × 1328 double-buffered texture set.
- The project owner physically reported perfectly smooth, flicker-free rendering with scrolling and other gestures working.
- A 243-second connected capture included approximately 63 seconds while Hades (Steam AppID `1145360`) ran locally. During that game window PhoneCast averaged 30.00 receive/decode FPS, 29.92 render FPS, 2.10 Mbps, 4.26 ms decode, 1.52 ms Vulkan submission, 1.06 ms queue age, 1.52% process CPU, and 102.33 MiB working set. Queue depth, transport drops/resyncs, and OpenVR-reported dropped/mispresented frames were all zero.
- Across the full active stream, dynamic periods averaged 29.11 receive/decode FPS and 28.36 render FPS. Working set stayed near 102.33 MiB rather than growing into the hundreds of MiB, and no Vulkan upload, `SetOverlayTexture`, or `VROverlayError_RequestFailed` error occurred.
- Hades is a locally running flat game. Steam briefly created an OpenXR test instance during launch, but it disconnected after about two seconds; this does not satisfy the pending standalone VR-scene coexistence test.

Recommended next validation:

1. Exercise portrait/landscape texture recreation and clean shutdown.
2. Validate coexistence with an actual standalone VR scene in both launch orders; the Hades run is flat-game evidence only.
3. Exercise reconnect, sleep/wake, notification, and longer lifecycle cases.
4. Treat dma-buf/zero-copy import as a later measured optimization; the measured CPU-RGBA/Vulkan staging path is already within the accepted performance envelope.

Deployment state:

- headset architecture/OS: AArch64 SteamOS, kernel `6.18.0-gfbdbca41fd45`;
- installed directory: `/home/steamos/phonecast`; the replaced raw-upload install is retained at `/home/steamos/phonecast.backup-20261004-222435`;
- Vulkan receiver process was started as PID `17725` after installation; PIDs are ephemeral and must be rechecked;
- logs: `/home/steamos/phonecast/receiver.log`; performance CSV: `/home/steamos/phonecast-performance-vulkan.csv`;
- SSH credential remains only in ignored local `.env` as `STEAM_FRAME_SSH_PASSWORD` and must never be logged or committed;
- use host alias `frame`; if connecting by the current DHCP address, `-o HostKeyAlias=frame` avoids stale-IP host-key ambiguity;
- local Vulkan package: ignored `out/packages/phonecast-steam-frame-arm64-vulkan.tar.gz`, SHA-256 `803cd0f0afd7df87d91dc3b1d30864b2d853d34f62d8068c0d8327ae2202603e`;
- installed receiver binary SHA-256: `b9b12e77c0b2262a9d3e33b3d3ae7c2034d798a7e8294766971495fadcaa46df`;
- the older Node-served raw-upload bundle is stale and must not be used for Vulkan validation.

Validation at handoff:

- Windows x64 build passes;
- all 11 Windows CTest tests pass;
- Linux ARM64 target cross-builds successfully and ARM64 portable tests pass under QEMU;
- physical V4L2 decoding and visible native streaming pass from the earlier raw-upload run;
- the Vulkan receiver initializes on the compositor GPU and is physically approved as smooth and flicker-free during phone interaction and the measured Hades window;
- the raw-upload flicker, memory-growth, and eventual submission-failure blocker is resolved for the measured run;
- rotation, clean shutdown, longer lifecycle cases, and actual standalone VR-scene coexistence remain pending.

## Objective

Remove the PC.

This sprint depends heavily on Sprint 0 findings.

Reuse:

```text
Networking
Protocol
Pairing
Input protocol
Configuration model
Streaming logic
Overlay model
Interaction logic
```

Replace:

```text
Windows-specific receiver
```

with:

```text
Steam Frame ARM64 receiver
```

Final architecture:

```text
┌──────── Android ────────┐
│                         │
│ MediaProjection         │
│      ↓                  │
│ Hardware H.264          │
│      ↓                  │
│ Network                 │
└───────────┬─────────────┘
            │
            │ Wi-Fi
            ▼
┌──────── Steam Frame ─────────────┐
│                                  │
│ Network Receiver                 │
│       ↓                          │
│ Hardware Decoder                 │
│       ↓                          │
│ GPU Texture                      │
│       ↓                          │
│ VR Overlay                       │
│                                  │
│         ┌─────────────┐          │
│         │    PHONE    │          │
│         └─────────────┘          │
│                                  │
│ Standalone VR game continues     │
│ underneath.                      │
└──────────────────────────────────┘
```

The PC should no longer be involved.

---

# Sprint 12 — Standalone UX

**Status:** `[x] Complete for the owner-accepted standalone baseline` — installation, manual dashboard launch, pairing/reconnect, native streaming, and PhoneCast-first VR-game coexistence accepted after physical use and a healthy follow-up log review; decoder recovery still needs physical fault validation, and Vulkan renderer stability/recovery remains follow-up work

- `[x]` SteamOS desktop-entry installer with absolute launch path and 48/128/256 pixel icons
- `[x]` ARM64 manifest/input packaging and executable-bit-preserving install path
- `[x]` Local download page provides copyable extraction/install commands and a short curl download-and-run bootstrap, with Linux/ARM64 and non-root checks, temporary-directory cleanup, and stop-on-failure behavior; automated page/route/shell and harmless bootstrap fixture checks pass, physical use of the improved flow remains pending
- `[x]` Per-user single-instance lock and owner-only local focus IPC
- `[x]` Second launch opens/focuses the resident PhoneCast dashboard
- `[x]` Dashboard-handle health check and recreation while the OpenVR runtime remains available
- `[x]` Owner-only persistent six-digit pairing credential with in-dashboard display
- `[x]` Manual LAN-address fallback and existing Android reconnect behavior retained
- `[x]` Launcher refuses to start SteamVR when the runtime is intentionally stopped
- `[x]` Uninstall path preserves user settings/pairing unless explicitly removed
- `[x]` Windows build/tests, Linux ARM64 cross-build, and ARM64 portable/runtime tests
- `[x]` Physical dashboard `+` discovery, first launch, and duplicate-launch prevention approved
- `[x]` Physical rotation, intentional dashboard Quit/relaunch, reconnect, notifications, and phone-lock/new-capture recovery approved
- `[x]` Follow-up video playback session reported issue-free; log review found no new decoder-busy error or sustained receive-without-decode failure
- `[x]` Bounded decoder-error/stall recovery implemented with keyframe resync, three reopen attempts, diagnostics, automated tests, and ARM64 cross-build
- `[ ]` Physically validate automatic recovery from the observed Qualcomm decoder-busy failure and the separate concurrent-download freeze case
- `[ ]` Deferred physical second-launch focus, dashboard recreation, headset sleep/wake, crash, and SteamVR restart checks
- `[x]` Physical PhoneCast-first coexistence over standalone Cubism; user approved normal gameplay and logs confirm an OpenXR VR scene with concurrent streaming
- `[~]` Game-active launch partially validated: after two Vulkan device-loss exits, a third PhoneCast launch while Cubism was already running streamed successfully until the game exited; this path is not stable enough to approve
- `[ ]` Add and physically validate bounded Vulkan renderer recovery after `VK_ERROR_DEVICE_LOST`/upload-fence timeout, and investigate whether PhoneCast's submission path can avoid the observed Adreno hangs
- `[ ]` Optional autostart control, only after the manual launcher is physically approved
- `[ ]` Automatic LAN discovery and cryptographic device identity/revocation

## Sprint 12 closure — 2026-10-05

The project owner requested closure after another issue-free session including
phone video playback, conditional on a healthy log review. Read-only SSH review
found no new decoder-busy, Vulkan submission, or sustained receive-without-decode
failure in approximately 1,000 newly appended one-second diagnostics samples.
The 599 dynamic samples (receive FPS at least 20) averaged 29.57 received FPS,
29.51 decoded FPS, 29.13 submitted FPS, 1.62% process CPU, and 105.54 MiB working
set (dynamic range 104.53–106.09 MiB). Rotation recreated portrait/landscape
textures, reconnect succeeded, and an intentional dashboard Quit at 19:41:04
was followed by a new native receiver at 19:41:09.

The latest overwritten CSV covers 169 seconds, including 167 connected samples.
Its 164 dynamic samples averaged 29.94 received FPS, 29.90 decoded FPS, and
29.27 submitted FPS. First submission was 88.47 ms; memory plateaued at
105.19 MiB in the final minute. Four video drops and two resyncs were confined
to startup and rotation samples, not sustained streaming. That CSV contains no
OpenVR dropped/mispresented sample. In the broader appended log, four samples
reported `vr-mispresented=1` with temporary higher submission cost, but no user
issue or sustained decode stall; these snapshots are not an isolated game-impact
benchmark or necessarily four distinct compositor failures.

Closure accepts the implemented manual-launch baseline and observed behavior,
not every original lifecycle criterion. Second-launch focus, dashboard
recreation, game-first standalone launch, headset sleep/wake, receiver crash,
SteamVR restart, and physical use of the new curl bootstrap remain explicitly
unvalidated. Autostart, discovery, and cryptographic identity remain unimplemented
follow-ups. Bounded decoder recovery was implemented after closure, but the
earlier two freeze incidents and logged V4L2 busy failure are not physically
confirmed fixed until the Qualcomm busy and concurrent-download cases are
reproduced. A later Cubism test produced two separate Adreno device-loss exits
outside that decoder policy; renderer recovery remains a focused follow-up. The
resize handle remains Sprint 14 work.

Validation at closure: Windows incremental build succeeded and all 11 CTest
tests passed; both local download-server/bootstrap tests passed. No receiver,
SteamVR, or game was restarted by the agent. Raw logs remain ignored under
`out/diagnostics/frame-feedback/`, with the prior snapshot retained separately.
This documentation closure introduces no native receiver code change or new
ARM64 cross-build claim.

## Physical feedback and log review — 2026-10-05

- The project owner reports that extraction required a terminal but installation worked; rerunning the installer caused no observed issue, PhoneCast appeared in the dashboard `+` app launcher, and a second launch did not duplicate the receiver. Explicit dashboard-focus behavior remains a separate check.
- Rotation, shutdown, reconnect, notifications, and phone-lock followed by a new capture/reconnect worked in physical use. Phone lock is not headset sleep/wake; crash recovery and SteamVR restart remain unvalidated. The newer curl bootstrap remains to be physically tested.
- The project owner reports that PhoneCast worked perfectly while playing standalone Cubism. Native SteamVR logs confirm PhoneCast started at approximately 19:15:46, followed by Cubism (AppID `804530`) becoming `VRApplication_OpenXRScene` at 19:16:47 and exiting at 19:21:07. This validates the PhoneCast-first launch order, not game-first launch.
- Approximately 259 one-second CSV samples overlap the Cubism scene. All remained connected, with zero transport drops/resyncs and zero sampled OpenVR dropped/mispresented frames. The 258 samples with receive FPS at least 20 averaged 29.48 received FPS, 29.47 decoded FPS, 28.59 submitted FPS, 1.62% process CPU, 102.61 MiB working set, and 1.29 ms queue age. These are sampled diagnostics, not isolated game-impact or glass-to-glass measurements.
- Usability feedback: lack of a directly grabbable screen-resize handle is frustrating. Add a bounded trigger-drag resize affordance in Sprint 14 rather than relying only on Settings scale controls.
- Two distinct freeze incidents were reported: (1) during a download, remote commands still affected the phone while displayed video froze, then video resumed when the download ended; (2) later, while trying to buy something from Steam with no download running, video froze and the project owner restarted the connection/receiver to recover. The second report does not retract the first. Exact incident timing, download device/application, and which restart was sufficient remain to be clarified.
- A separate Chromium + PhoneCast + Cubism failure was investigated from read-only Frame logs. The kernel recorded repeated Adreno `a6xx_irq` GPU faults and recovery, Cubism's OpenXR client reported Vulkan `VK_ERROR_DEVICE_LOST` (`-4`), and PhoneCast exited through its fatal render-error path after its upload fence returned `VK_TIMEOUT` (`VkResult 2`). The first GPU fault occurred after Chromium started but before the reviewed Cubism launches, and further GPU faults occurred during a later Cubism run while PhoneCast was absent, so that earlier evidence did not establish PhoneCast as the cause. The decoder remained near 30 FPS immediately before the render failure; decoder recovery does not address this event.
- A later decoder-recovery-build test reproduced two PhoneCast exits during Cubism coexistence at approximately 20:17:35 and 20:20:09. In both cases the receiver was still receiving, decoding, and submitting video before reporting `Vulkan frame upload failed (VkResult -4)` (`VK_ERROR_DEVICE_LOST`). One second earlier, the kernel logged an Adreno `a6xx_irq` fault and hangcheck recovery naming `phonecast-vr-stream-receiver` as the offending task. No PhoneCast coredump was produced; it followed the existing fatal renderer-error path. Decoder-recovery counters remained zero. This is stronger evidence involving PhoneCast's GPU submission path, but it does not by itself distinguish an application synchronization defect from a SteamOS/Turnip driver fault.
- The third PhoneCast launch in that test started while Cubism was already active and streamed until Cubism exited. Across 167 dynamic overlapping samples it averaged 29.74 received/decoded FPS, 29.20 submitted FPS, 1.55% process CPU, 102.16 MiB working set, and 1.14 ms queue age, with no dynamic transport drops/resyncs or sampled OpenVR dropped/mispresented frames. This establishes functional game-active launch, not stable game-first approval because of the two preceding GPU faults.
- Preserve the captured evidence under ignored `out/diagnostics/frame-test-20261005-2025/`. Circle back in a focused renderer-reliability task after collecting more launches with multiple games and controlled launch orders. Implement bounded renderer recreation for recoverable device loss/timeout and separately investigate preventing the GPU hang; renderer recovery cannot guarantee game survival during a global GPU reset.
- Log review found an earlier `Queueing H.264 access unit: Device or resource busy` error followed by 84 consecutive diagnostics samples with roughly 30 received FPS but zero decoded/submitted FPS. This establishes native decoder failure in that session; correlation to either reported incident remains pending, and the incidents must not be assumed to share a cause. The Cubism session contains no such error. Bounded decoder-error and sustained-stall recovery is now implemented without closing the remote-input connection: queued prediction frames are discarded, a fresh keyframe is requested, and the decoder is reopened up to three times. Automated tests and the ARM64 cross-build pass, but physical recovery from the Qualcomm busy failure remains pending; retain concurrent-download testing as a separate reproduction case.
- Logs were inspected over SSH without restarting PhoneCast, SteamVR, or the game. Raw local evidence is ignored under `out/diagnostics/frame-feedback/`; credentials were not printed or added to tracked files.

## Session Handoff Snapshot — 2026-10-05

Branch and recent commits:

- branch: `review/sprint-12-standalone-ux`;
- `32859b8` — Android privacy display mode added to the backlog;
- `900c69d` — Sprint 12 Steam Frame bundle served by the local package page;
- `e157251` — Sprint 12 manual launcher baseline, single-instance IPC, dashboard recovery, and persistent pairing.

Research documentation added in this handoff:

- [`docs/frametop-research.md`](docs/frametop-research.md) records a source review of [`DeeJanuz/frametop`](https://github.com/DeeJanuz/frametop) at commit `ca9492be000a24f8e539607a02ee5d12f41801a6`;
- no PhoneCast source code, build configuration, package, or hardware state was changed by that research;
- the research repository was inspected but not installed or physically run by PhoneCast.

Most useful Frametop findings:

1. Frametop imports Linux DMA-BUFs into SteamVR with `IVRIPCResourceManagerClient::ImportDmabuf` and submits `TextureType_SharedTextureHandle`. This is a credible future experiment for removing PhoneCast's remaining V4L2 NV12 → CPU RGBA → Vulkan staging copies, but Frametop proves XRGB/ARGB compositor buffers rather than Qualcomm decoder NV12. Format/modifier compatibility, V4L2 export, buffer ownership, synchronization, orientation, and teardown remain unknown.
2. Frametop avoids a permanent game-controller claim by predicting when a controller's render-model laser tip intersects a panel and enabling `MakeOverlaysInteractiveIfVisible` only while aimed, pressed, or in a short linger interval. This is a promising Sprint 14 experiment, not a validated replacement for PhoneCast's dashboard-only interaction.
3. Its separate move bar, drag-depth adjustment, click stabilization, release catcher, device-loss cleanup, wrist/head placement, custom keyboard fallback, and attention-based scheduling are useful interaction references.
4. Frametop's local KDE/wlroots desktop, gaze/hand systems, virtual controller, input relay, and undocumented vrserver WebSocket are not appropriate dependencies for PhoneCast.
5. Frametop is MIT-licensed and compatible with PhoneCast's MIT license; substantial copied code would still require retaining DeeJanuz's copyright and license notice.

Decisions at handoff:

- do not alter the approved native Vulkan renderer or dashboard-first UX based only on source review;
- do not pull DMA-BUF work into Sprint 12 or treat it as required for v0.1—the measured Vulkan path is already physically accepted;
- finish the existing Sprint 12 physical validation matrix before optional optimization or Sprint 14 interaction experiments;
- keep any later DMA-BUF/shared-handle path behind the Steam Frame/OpenVR platform boundary with runtime capability checks and the Vulkan renderer as fallback;
- keep standalone VR-scene coexistence in both launch orders explicitly unvalidated until PhoneCast itself is physically tested.

Historical next work at this handoff (superseded by the closure decision above;
unvalidated items remain follow-ups):

1. Install the current Sprint 12 bundle on Steam Frame and validate dashboard `+` discovery, first launch, and second-launch focus.
2. Exercise rotation, clean shutdown, reconnect, notifications, sleep/wake, crash recovery, and SteamVR restart.
3. Test a real standalone VR scene in both PhoneCast-first and game-first launch orders.
4. Only after that baseline is accepted, consider a bounded DMA-BUF capability probe and the Sprint 14 aim-gated interaction experiment described in the research document.

Once standalone operation works, optimize PhoneCast specifically for Steam Frame.

## Approved launch model

PhoneCast should be installed as a user-launchable application in the SteamVR dashboard's **`+` app launcher**. Automatic startup must not be required for normal v0.1 use.

Primary behavior:

1. The user opens the SteamVR dashboard and selects **`+`**.
2. The user selects PhoneCast.
3. If PhoneCast is not running, the launcher starts the native receiver.
4. If PhoneCast is already running, the launch request signals the existing process and opens or focuses its dashboard instead of creating a duplicate receiver.
5. The user connects or reconnects the trusted phone and uses Show, Glance, Pin, Settings, and placement controls normally.
6. Closing or hiding the PhoneCast UI must not stop the receiver unless the user explicitly chooses Quit.

Autostart may be offered as an explicit opt-in setting after the manual launcher is reliable. It must default off, remain independently reversible, and must not be necessary to recover PhoneCast.

Desired experience:

```text
Put on Frame

       ↓

Open SteamVR dashboard

       ↓

Press + and select PhoneCast

       ↓

PhoneCast starts or focuses its existing instance

       ↓

Trusted phone reconnects

       ↓

Launch VR game and play normally

       ↓

Open PhoneCast dashboard

       ↓

Select Show / Glance / Pin

       ↓

View, interact, and dismiss

       ↓

Continue playing
```

The user should not need to launch a receiver from SSH, a terminal, or a development computer during ordinary use.

## Implementation scope

Investigate and implement:

- native application registration and visibility in the dashboard `+` launcher;
- a SteamOS `.desktop` launcher using an absolute executable path and suitable 48, 128, and 256 pixel icons;
- correct ARM64 application manifest and executable-bit packaging;
- a single-instance lock;
- a small local signal/IPC path so a second launch focuses or opens the resident PhoneCast dashboard;
- `LaunchDashboardOverlay` and `ShowDashboard` behavior where appropriate;
- dashboard-handle health checking and recreation if SteamVR drops the dashboard overlay while the receiver remains alive;
- trusted-device pairing and credential persistence;
- automatic discovery where reliable, with a manual LAN-address fallback;
- reconnect after network interruption;
- SteamVR restart, dashboard restart, headset sleep/wake, game switching, receiver crash, and intentional-quit behavior;
- clear logs and an uninstall/disable path;
- optional autostart using `SetApplicationAutoLaunch`/`GetApplicationAutoLaunch` or a user-level service only after the manual launch flow passes physical testing.

Do not use undocumented SteamVR UI injection. The launcher should use supported application registration and desktop-entry behavior. A third-party native Steam Frame utility demonstrates the dashboard `+`, single-instance, and optional service patterns, but PhoneCast must validate them with its own dashboard, networking, sleep/wake, and application transitions.

## Acceptance Criteria

- A normal user can install PhoneCast and find it through the SteamVR dashboard `+` launcher.
- Selecting PhoneCast starts the native receiver without SSH or a terminal.
- Selecting PhoneCast again while it is running does not create a second receiver and instead opens or focuses PhoneCast.
- The dashboard icon and controls recover if SteamVR recreates or loses the dashboard overlay.
- Manual address entry remains available if discovery fails.
- A trusted phone reconnects without repeating first-time pairing during ordinary restarts.
- SteamVR restart, headset sleep/wake, game switching, network loss, receiver crash, and intentional Quit have documented and physically tested outcomes.
- Optional autostart defaults off and can be enabled and disabled from a supported PhoneCast control.
- Launching PhoneCast does not start SteamVR unexpectedly when the runtime is intentionally stopped.
- Packaging preserves executable permissions, absolute paths, manifests, icons, and user data across an update.
- The launch flow is physically validated both before and during a standalone VR scene.

Optional dashboard-closed hotkeys are not required for Sprint 12 standalone UX.

---

# Sprint 13 — Optional Phone Audio

**Status:** `[x] Complete — native headset playback, lip-sync, local mute, standalone-game mixing, controls, lifecycle, and sustained stability owner-approved`

- `[x]` Project owner chose Sprint 13 next; Sprint 12 decoder fault validation and Vulkan recovery/investigation remain deferred follow-ups
- `[x]` Android playback-capture restrictions and timestamp APIs researched
- `[x]` Windows shared-mode output and Linux PulseAudio/PipeWire candidates researched
- `[x]` Initial PCM baseline, capability-negotiation requirement, bounded queues, A/V timing, and validation matrix documented in `docs/phone-audio.md`
- `[x]` Negotiated audio protocol and tested cross-version video-only fallback
- `[x]` Explicit default-off Android opt-in, permission, playback-only recorder, and cleanup implementation
- `[x]` Optional local-phone media mute works with continued headset capture and restores when the stream closes; projection-revoke and unclean-exit recovery remain documented follow-ups
- `[x]` Independent bounded audio transport and output workers with stale-sound rejection
- `[x]` WASAPI shared-mode and runtime-loaded PulseAudio/PipeWire output implementations; transactional headset mute/volume/route controls and explicit CLI device selection
- `[x]` Read-only native audio-server/library/sink probe and isolated Pulse null-sink backend tests
- `[x]` Decoded picture timestamp preservation and tested bounded A/V synchronization/fallback policy
- `[x]` Audio-enabled video starvation reproduced and corrected with per-picture deadlines, sufficient bounded retention, overflow progress, and new timing diagnostics; continuous-cadence regressions pass
- `[x]` Corrected receiver physically approved after flawless 15–20 minute two-video playback, seeking/pause/resume/2× playback, and standalone-game coexistence
- `[x]` Windows build/all 12 CTest tests, Android JVM tests/APK/lint, ARM64 build and QEMU portable/audio/runtime tests
- `[x]` Supported playback, perceived lip-sync, game-audio coexistence, reconnect/rotation lifecycle, receiver controls, and stream-close volume restoration owner-approved
- `[~]` Protected/capture-blocked sources and every local-volume failure/recovery path retain design/automated coverage but were not each physically exercised before owner-approved closure

The implementation uses PCM S16LE at 48 kHz stereo in 10 ms blocks: 1.536 Mbps of samples, approximately 1.571 Mbps with audio framing before TCP/IP overhead. The project owner closed Sprint 13 after physically confirming perfect perceived lip-sync through pause/resume, seeking, and 2× playback; simultaneous standalone-game and phone audio; working local-phone mute and stream-close restoration; receiver controls and lifecycle behavior; and flawless playback of two videos over approximately 15–20 minutes. Read-only diagnostics corroborate sustained near-30 FPS video with no audio-output failures or sampled compositor drops. This approval is qualitative rather than an external click/flash skew distribution and does not claim every protected-source or failure-recovery case was physically exercised. Native PipeWire/PulseAudio environment evidence, cadence corrections, remaining limitations, and exact closure diagnostics are recorded in `docs/phone-audio.md`.

## Objective

Optionally capture supported Android playback audio and route it through the active VR audio output. This remains outside the initial visual MVP and must be user-controlled.

Investigate and document:

- Android AudioPlaybackCapture restrictions and applications that prohibit capture;
- audio codec and transport choices;
- A/V synchronization and latency;
- reconnect and orientation/session behavior;
- Windows and Steam Frame audio-output selection;
- mixing with game audio without disrupting the VR application's device;
- mute, volume, and audio-disabled defaults.

Do not capture microphone input or protected audio, and do not imply that every Android application permits playback capture.

### Acceptance Criteria

- User can explicitly enable or disable phone audio.
- Supported phone playback is heard through the selected headset/VR output.
- Audio remains acceptably synchronized with the phone picture.
- Unsupported or protected playback fails clearly without breaking video streaming.
- Phone audio does not unexpectedly replace or mute game audio.

## Sprint 13 closure — 2026-10-06

The project owner explicitly approved Sprint 13 after perfect perceived lip-sync
through seeking, pause/resume, and 2× playback; simultaneous phone video/audio over
a standalone VR game; working receiver controls and reconnect/rotation lifecycle;
local-phone mute with stream-close volume restoration; and two flawless videos over
approximately 15–20 minutes. A final read-only 1,257-second snapshot contained 871
dynamic samples averaging 29.78 received/decoded and 29.70 submitted FPS, with no
audio-output failure, decoder recovery, or sampled OpenVR drop/mispresent.

Closure does not convert untested protected-source behavior, external skew
measurement, Windows audio, unavailable-device recovery, projection-revoke restore,
headset sleep/wake, or Android force-kill recovery into tested claims. Reddit can
produce capturable audio for embedded videos shown muted because source UI mute is
not exposed by Android playback capture. Those limitations remain documented in
`docs/phone-audio.md` and do not block progression. The next planned sprint is
Sprint 14 — Gestures and UI; Sprint 12 decoder/Vulkan reliability follow-ups remain
separate deferred work.

---

## Sprint 13 session handoff — 2026-10-05, audio/video jitter follow-up

### Current request and acceptance state

The owner requested a design for the remaining audio/video jitter fix, then
requested this handoff before usage runs out. **Design and implement the next
focused Sprint 13 timing correction; do not start Sprint 14.** No second correction
has been implemented or approved. The first correction improves playback, but the
owner reports significant video jitter and worsening responsiveness with more
actions. They also observed a glitched YouTube mini-player on the physical phone;
retain that as a separate observation, not proof of an Android or receiver cause.
Sprint 13 remains open; smoothness, lip-sync, routing, and game mixing are not
accepted merely because builds/tests pass.

### Repository and built/download state

- Branch `main`, ahead of `origin/main` by two commits. Latest commits remain
  `df4295e` (installer line endings) and `0166116` (bounded decoder recovery).
- Original Sprint 13 implementation and the first video-cadence correction are
  still modified/untracked in the working tree; **no commits were made**. Preserve
  these files. `git diff` alone omits untracked audio source/tests/docs.
- Main references: `docs/phone-audio.md`, `docs/development.md`,
  `docs/performance.md`, `core/include/phonecast/core/audio/AudioPlayback.h`,
  `core/src/AudioPlayback.cpp`, `tests/audio_tests.cpp`, and
  `apps/stream-receiver/src/vr_main.cpp`.
- Android Sprint 13 APK is unchanged by the receiver-only correction:
  `android/sender/app/build/outputs/apk/debug/app-debug.apk`.
- Windows executable: `out/build/windows-x64/bin/phonecast-vr-stream-receiver.exe`.
- ARM64 build: `out/build/linux-arm64-sprint13`; current bundle:
  `out/packages/phonecast-steam-frame-arm64-sprint13.tar.gz` (551,833 bytes).
- Bundle SHA-256:
  `7c5f5132d554a9a017c96ac2296ab16f1bb6477459d1575eefcc46b328ce259b`.
- Packaged/installed receiver SHA-256:
  `661e6d1f06feebc77d536976362c3e0199c4ffe77df3d12cf65035a14137f177`.
- Existing Node server at `http://10.0.0.3:8080` serves that bundle through both
  `/phonecast-steam-frame-arm64-sprint13.tar.gz` and the stable
  `/phonecast-steam-frame-arm64.tar.gz` alias. Both live downloads were hash-verified;
  bootstrap script matches the local file. Artifact responses use `no-store` and
  read files per request, so replacing the archive needs no server restart.
- Packaging preserves executable bits and LF installer scripts, excludes secrets,
  and uses stripped ARM64 binaries. Quit PhoneCast before physical installation.

### First correction: implemented, software-tested, physically insufficient

Original audio-on logs showed about 29.5 decoded FPS but only 5.2 submitted FPS;
without audio, about 29.8 decoded/27.1 submitted FPS. A three-picture queue could
continually evict its oldest picture before the 100 ms timeout. Timeout also
flushed future pictures by jumping to newest, reducing cadence.

The first correction changes `AudioVideoQueue` to a hard eight-picture ceiling,
independent 100 ms arrival-based deadlines, due-only releases/coalescing, and
forced progress on capacity overflow. Audio-off/unknown/incompatible clocks still
use immediate newest-picture presentation. Audio buffering, protocol, Android
capture, Vulkan, and headset services were not changed. It adds console/CSV
`video_sync_*` queue/deadline/overflow/coalescing/last-hold/estimated-skew fields.
Skew is picture PTS minus estimated audible PTS, not measured lip-sync; the validity
flag is only a nonzero/two-second-domain guard, not verified clock compatibility.

Validation passed: all 12 Windows CTest tests, audio tests repeated five times,
ARM64 cross-build plus QEMU portable/audio/standalone-runtime tests, and both local
server/bootstrap tests. A deterministic exact 30 FPS/130 ms audio-lag simulation
presents 297/300 pictures in ten seconds, with three still pending and maximum
steady-state gap 34 ms. Tests also cover 20/60 FPS, multiple clock lags, 240 FPS
capacity pressure, clock stalls/jitter/transitions, reset, and arithmetic overflow.
The earlier standalone diagnostic changes from zero to 300/303 submissions.
**Those simulations did not model the complete runtime dispatch/decode path.**
An existing asynchronous test race was corrected by waiting for the audio clock,
not assuming submission immediately implies that latency has been observed.

### Latest read-only physical evidence

Ignored snapshot directory:
`out/diagnostics/frame-audio-followup-20261005-223914/` contains `receiver.log`,
`performance.csv`, `vrserver.txt`, `vrcompositor.txt`, `read-only-probe.txt`,
`comparison.json`, and `findings.md`. Earlier starvation evidence/reproduction is
under `out/diagnostics/frame-audio-20261005-2157/`, including
`cadence-fix-validation.json` and `queue-reproduction.cpp`.

The installed receiver hash matches the first correction. It ran from 22:16:10
until an **intentional dashboard Quit at 22:37:04**. At the 22:39:18 probe no receiver
was running. PIDs are ephemeral; the reviewed process was 72101. Latest cast was
approximately 22:32:44–22:37:04. Nothing was started, stopped, or reconfigured by
the agent on the headset.

- Latest dynamic audio windows (188 samples, receive FPS >=20 and audio bitrate
  >1 Mbps): 31.132 received FPS, 31.025 decoded FPS, **25.000 submitted FPS**.
  Thirty-one samples were below 20 submitted FPS; worst was 7.994 FPS.
- Receive windows burst as high as 94.949 FPS. These are one-second arrival rates,
  not proof that the sender continuously encodes above its configured 30 FPS.
- Integrating all latest connected windows gives approximately 6,294 decoded
  pictures versus 5,059 submitted (gap ~1,235), but only 48 sync-queue coalesced
  pictures and zero sync overflow releases. Session/config clears and ending
  pending pictures explain some differences; per-pass attribution is absent.
- **Important code path:** `vr_main.cpp` drains `while (server.Pop(...))`, decodes
  each message, repeatedly overwrites `latestFrame`, then adds only the final
  picture to `videoPlayout`. Most observed picture loss is therefore before the
  corrected queue; its continuous-source unit tests cannot cover this behavior.
- Last-picture hold observations often reach the 100 ms fallback deadline:
  mean 91.788 ms, maximum 126.980 ms in those dynamic windows. This is not a
  measured frame-gap distribution; render-loop work/polling can overshoot deadlines.
- Audio drop counter ended at 4,088 blocks versus 25,498 submitted across the
  whole run; audio failure counter stayed zero. Latest cast logged 22 distinct
  successful Pulse output descriptions (initial plus 21 further output instances),
  naming successively created `PhoneCast VR.<id>.spatialize_filter_chain.capture`
  sinks, without phone disconnects between them.
- `PulseAudioOutput::Description()` is stored after Open, so those changed
  descriptions indicate repeated output recreation. Core currently closes/reopens
  and resets its clock whenever accepted audio PTS jumps by more than 100 ms.
  This is a plausible reset mechanism, **not a proven per-event reason**: reset
  reasons, timestamp gaps, sequence gaps, and clock jumps are not logged yet.
  Capture scheduling, transport drops, controls/configuration, and source behavior
  must not be ruled out without evidence.
- No new receiver Vulkan/upload/device-loss failure or decoder recovery trigger.
  Recent filtered kernel probe returned no GPU-fault lines. Sampled compositor
  drops were zero, but some frame-index snapshots repeated; these do not establish
  absence of frame-level compositor jitter. No Android logs were collected, so
  the phone-side YouTube mini-player observation remains undiagnosed.

### Proposed next design — not implemented, finalize before coding

1. **Bound work and preserve presentation opportunities.** Replace unbounded
   transport draining with a bounded decode/dispatch pass (provisional packet/time
   budgets; a single decoder call may exceed a soft time budget). Feed produced
   pictures into playout instead of silently overwriting every intermediate one.
   Service due presentation and interaction between passes. Keep codec config,
   rotation, recovery, and H.264 prediction order correct: never arbitrarily drop
   compressed dependent access units to meet a budget. Coalesce decoded pictures
   deliberately when stale/overdue; retain bounded memory and prioritize game health.
   Verify the portable decoder contract before introducing any worker-thread split;
   a large threading refactor is not the first choice.
2. **Make timing stable rather than follow raw arrival bursts.** Use a bounded,
   monotonic media-time/steady-clock mapping and explicit clock-validity states.
   Smooth small timing observations with bounded correction; interpolate only
   within submitted/known playable sound, never indefinitely beyond it. When sound
   is unavailable or timing is unreliable, fall back to a stable video schedule
   rather than repeatedly changing between immediate frames and 100 ms holds.
   Keep audio-off immediate and cap additional video latency. Thresholds/hysteresis
   are provisional until measured; do not simply enlarge queues or wait budgets.
3. **Separate audio packet gaps from device failure.** Do not recreate Pulse/WASAPI
   output for every ordinary missing/late block. Detect gaps using both sequence
   and PTS, discard stale application audio, reanchor timing, and use bounded
   silence only where appropriate. Large/new-epoch timeline changes must not play
   stale backend sound: design an output-worker flush/reset operation or a justified
   bounded reopen, rather than assuming clearing the application queue flushes the
   device. Reopen remains appropriate for actual backend failure/device changes.
   Mute, opt-out, permission/session stop, and routing must retain immediate cleanup.
4. **Instrument before claiming cause.** Add bounded aggregate counters/timings
   for decode-pass size/duration, pre-playout coalescing, presentation intervals,
   audio sequence/PTS gaps, reset/reanchor/reopen reasons, and backend open duration.
   Separate stale/overflow/duplicate audio drops. Include event/input/dispatch loop
   timing to establish why more actions worsen responsiveness. Do not log touch
   contents, notification text, screen/audio samples, or credentials.
5. **Regression coverage must exercise orchestration.** Reproduce bursty arrivals,
   multiple decoded pictures per pass, interaction/notification work, variable
   decoder cost, audio packet losses and clock reanchors—not just an evenly spaced
   `AudioVideoQueue` source. Check sustained presentation cadence and inter-frame
   gap distributions, bounded queues/waits, timeline monotonicity, no stale replay,
   and no backend-recreation storm under ordinary gaps. Preserve existing video-only,
   network/keyframe, rotation/reset, audio-controls, and decoder-recovery tests.
6. **Validate incrementally.** Build/all Windows tests, ARM64 build/QEMU tests,
   private Pulse null-sink tests if backend reset semantics change, then publish a
   verified receiver bundle. Avoid an Android change unless timestamp evidence
   justifies it; if needed, collect capture timestamp/fallback diagnostics and run
   JVM/APK/lint. Owner retest: same video audio-off/on, then scrolling/notification/
   rotation under playback, click/flash sync, and game coexistence. Record FPS/gap,
   audio resets/drops, memory, and owner approval. No simulated result closes Sprint 13.

Build notes: Windows CMake/CTest must be invoked through PowerShell and WinLibs
runtime prepended as documented in `docs/development.md`. Linux caches use `/src`;
reuse them only in Docker mounted there, with `MSYS_NO_PATHCONV=1`, never Windows
CMake. Debian prerequisites include `cmake ninja-build g++-aarch64-linux-gnu git
ca-certificates file libpulse-dev qemu-user`; QEMU uses
`qemu-aarch64 -L /usr/aarch64-linux-gnu`. SSH uses host alias `frame` and credentials
from ignored `.env`; do not print/copy credentials into tracked files. Do not
restart SteamVR through RDP or restart/reconfigure headset audio services. Deferred
Sprint 12 decoder-fault validation and Vulkan hang/recovery remain separate work.

---

## Sprint 13 implementation continuation — review branch

The previously uncommitted Sprint 13 baseline and first cadence correction are
preserved as `fece424` on `review/sprint-13-av-timing`. The next receiver-only
correction now implements bounded video/audio dispatch, a presentation opportunity
for every decoded picture, bounded monotonic audible-clock interpolation, and
worker-side backend flush/reanchor instead of stream recreation for forward audio
gaps above 100 ms. New aggregate diagnostics expose dispatch/loop/submission gaps,
audio sequence/PTS gaps, flushes/opens, and classified drops.

Windows build/all 12 CTest tests pass; ARM64 build and QEMU portable/audio/runtime
tests pass. Private Pulse null-sink tests pass including flush/write-after-flush.
The rebuilt receiver bundle and hashes are recorded in `docs/phone-audio.md`.
No hardware receiver, SteamVR, or audio service was restarted during implementation.
In a subsequent short native-headset test, the project owner reported audio was
“way better” and video looked good. Read-only review of 33 dynamic audio-enabled
CSV samples averaged 28.63 received, 28.60 decoded, and 28.57 submitted FPS, with
zero dynamic transport drops/resyncs, audio failures, or sampled OpenVR dropped/
mispresented frames. The run used one output open with no reanchor/flush/recreation
storm. This is encouraging initial physical evidence, not final smoothness or
lip-sync approval.

Simultaneous sound from the phone and headset is expected: Android playback capture
copies eligible sound, and PhoneCast intentionally does not change phone media
volume or audio focus. An optional local-phone mute is now implemented behind a separate Android setting:
it saves media volume before muting, restores on disconnect/stop/teardown, and uses
a persisted marker for next-process-start recovery after an unclean exit. Android
cannot execute cleanup at the instant its process is forcibly killed. A later physical test confirmed that local media volume zero does not silence capture
on the target phone: the mute worked, streaming remained stable, and YouTube playback
was watchable. The approximately 1,619-second CSV included 546 dynamic audio/video
samples averaging 29.62 received, 29.61 decoded, and 29.57 submitted FPS, with no
dynamic audio-output failure or sampled OpenVR dropped/mispresented frame. Exact
restoration across disconnect, Stop, projection revoke, checkbox disable, and an
unclean process restart remains pending.

The same test found that muted embedded Reddit videos can still produce captured
audio. Android selects playback capture by usage, source UID, and capture policy and
does not expose the source UI's per-video mute state. PhoneCast therefore cannot
infer this state from nonzero PCM; a future package/UID blocklist could exclude an
application entirely. Lip-sync, routing, protected playback, game mixing, and the
remaining lifecycle validation remain pending; stable fallback scheduling and clock-state hysteresis
remain possible further tuning. This does not close Sprint 13 or begin Sprint 14.
The previous handoff above is historical implementation state.

---

# Sprint 14 — Gestures and UI

**Status:** `[~] In progress — mockup-driven dashboard/settings refresh started; hardware approval pending`

- `[x]` Local UI mockups reviewed and `_injest/` excluded from version control
- `[x]` Framecorder dashboard-closed hotkey source research completed and documented
- `[~]` Mockup-derived settings refresh and landscape quick dashboard implemented for build validation
- `[x]` Removed Glance/Pin from the primary dashboard after physical feedback found no useful visible distinction
- `[x]` Dashboard Show now reveals the phone head-locked beside the quick panel before normal Show/Hide operation
- `[~]` Phone lower control area now has Back, Close, Move, and a trigger-drag resize corner; settings/notification consistency remains pending
- `[ ]` Trigger-drag depth adjustment and complete drag cancellation/release safety
- `[ ]` Keyboard/text-entry path and PhoneCast-owned fallback decision
- `[ ]` Portable opt-in gesture recognizer and gameplay false-activation evidence
- `[ ]` Narrow opt-in hotkey experiment and game-input coexistence validation
- `[ ]` Native Steam Frame physical validation in both launch orders

The initial visual implementation follows the two owner-provided `_injest` mockups for panel hierarchy, dark navy cards, blue selection states, settings categories, calibration sliders, transactional actions, and bottom move affordances. Physical feedback found the portrait quick panel too tall and found no useful visible distinction for its Glance and Pin actions. The quick panel is therefore landscape, exposes only Show/Hide, concrete placement choices, Android Back, Settings, and Quit, and positions a newly shown phone beside the dashboard. Glance/Pinned remain internal compatibility states for existing keyboard/dormant controller paths but are no longer presented as primary dashboard modes. The phone footer adds a dedicated lower-right resize corner while keeping Back, Close, and Move separate. These local reference images are intentionally ignored rather than shipped. Compilation and automated tests do not constitute visual approval; the refreshed surfaces still require headset review and iteration.

Framecorder research at native commit `171a905006bb917e5a1677fa922a1d00cc1ec214` found a narrower hotkey implementation than the earlier overlay-global reference: one optional Boolean action, a left-thumbstick binding-layer long press, haptic feedback, and normal action-set priority `0`. PhoneCast will test a separate narrow opt-in action set rather than reactivating its dormant broad global set. See [`docs/framecorder-hotkey-research.md`](docs/framecorder-hotkey-research.md).

## Objective

Make PhoneCast controls feel consistent with native Steam Frame application and window behavior, and turn the currently disabled or unreliable convenience controls into deliberate, safe, physically validated interactions.

This sprint should improve the existing UI rather than create a second settings or placement model. Dashboard, scene-panel, gesture, hotkey, and button actions must all dispatch the same portable commands and persist through the existing settings model.

## Native-style panel controls

Give every applicable PhoneCast panel a consistent control area below its content, modeled on Steam Frame's first-party app/window presentation where public APIs permit it.

The lower control area should expose context-appropriate actions such as:

- move/position;
- resize or scale, including a directly grabbable screen-resize handle (project-owner feedback: Settings-only resizing is frustrating);
- pin/unpin;
- hide/close;
- settings;
- keyboard/text entry where the active panel supports text input.

The phone panel should provide an obvious keyboard option without permanently obscuring phone content. Opening and closing the keyboard must not leave the underlying game or phone with a stuck pointer/button state. Determine whether the SteamVR keyboard can be invoked reliably by a native ARM64 overlay; if it cannot, implement a portable PhoneCast keyboard surface rather than depending on undocumented UI injection.

Do not copy private Steam Frame assets or patch Valve UI. Reproduce the interaction pattern with supported OpenVR surfaces and PhoneCast-owned visuals.

## Move-bar interaction

Standardize the move bar across phone, settings, notification-preview, and other movable PhoneCast panels:

- hold the move bar with the controller trigger to move the panel;
- move the controller to reposition and orient it;
- while the move bar is held, push the joystick up to move the panel farther away and down to bring it closer;
- clamp distance and scale to safe ranges;
- release the trigger to commit the placement;
- cancel, tracking loss, dashboard closure, or focus loss must release the drag cleanly without a jump or stuck input state.

Distance adjustment should follow the active view ray or panel-depth axis, not world vertical. The portable interaction layer should own bounded adjustment semantics while the OpenVR backend supplies controller poses and axis events.

## Gestures

Replace the dormant binary wrist-pose experiment with a physically tuned, opt-in recognizer based on the existing gesture research:

- deliberate raise-and-turn sequence rather than a broad static pose;
- filtered pose samples, entry/exit hysteresis, candidate-hand lock, short tracking-loss grace, and cooldown;
- left, right, or either-hand preference;
- conservative, balanced, and responsive sensitivity choices;
- visible progress only after an intentional candidate is recognized;
- dashboard access remains available when gestures are disabled or fail.

Gesture handling must remain platform-independent above pose sampling. Record false activations, failed attempts, activation time, and tracking-loss behavior during real gameplay before enabling gestures by default.

## Hotkeys and controller buttons

Repair and simplify optional controller shortcuts without restoring the old broad overlay-global action set.

- Start with a narrow, user-remappable, opt-in action set containing only approved PhoneCast commands.
- Test dashboard-open and dashboard-closed behavior separately on native Steam Frame and Windows/VRLink.
- Never claim trigger, grip, joystick axes, or other gameplay controls globally merely to make PhoneCast convenient.
- Disable shortcut actions while the dashboard or a PhoneCast text-entry surface owns input where necessary.
- Surface whether each shortcut is bound and active; manifest or binding load success alone is not proof that it works.
- Retain dashboard controls as the reliable recovery path.

Candidate shortcuts include show/hide, glance, pin/unpin, and opening PhoneCast controls. Exact defaults must be chosen only after conflict testing against real games.

### Framecorder dashboard-closed hotkey research

- `[x]` Reviewed [`coah80/framecorder`](https://github.com/coah80/framecorder) at commit `171a905006bb917e5a1677fa922a1d00cc1ec214`; see [`docs/framecorder-hotkey-research.md`](docs/framecorder-hotkey-research.md).
- Framecorder uses one optional Boolean action, a binding-layer long press on the left thumbstick button, one haptic action, and normal action-set priority `0`; it does not claim trigger, grip, or thumbstick axes for this shortcut.
- The project owner physically tested Framecorder and reports that its hotkey is available while the dashboard is closed. This is user-reported Framecorder evidence, not validation of PhoneCast shortcuts.
- Compared with the overlay-global approach in [`docs/frame-passthrough-shortcuts-research.md`](docs/frame-passthrough-shortcuts-research.md), the normal-priority result is a better first native PhoneCast experiment but still requires per-action activity diagnostics and real-game conflict testing.
- Keep this as Sprint 14 work, not a Sprint 12 dependency. Do not reactivate PhoneCast's dormant broad global action set; any resulting shortcut experiment must be opt-in, narrowly bound, and physically checked for game-input conflicts.

## UI consistency and feedback

- Use one visual language and control ordering across the dashboard, phone toolbar, settings, notifications, and keyboard.
- Keep controls below panels where practical so streamed content and touch targets are not covered.
- Provide clear hover, pressed, dragging, disabled, connected, and permission-required states.
- Make hit targets usable with the Frame controller laser and avoid hover-only destructive actions.
- Render panels only when visible or dirty; UI polish must not add continuous redraw or measurable game-frame impact.
- Preserve transactional Apply/Cancel behavior for persisted settings.
- Ensure all panels recover after dashboard recreation, SteamVR restart, sleep/wake, and application transitions.

## Acceptance Criteria

- PhoneCast panels use a consistent lower control area that is physically approved on Steam Frame.
- The phone panel offers working keyboard/text entry, or a clearly documented platform limitation with a tested PhoneCast-owned fallback.
- Holding a panel's move bar allows controller repositioning and joystick-controlled farther/closer depth adjustment.
- Drag release, cancellation, focus loss, tracking loss, and dashboard closure never leave stuck input or cause an uncontrolled placement jump.
- The wrist gesture opens PhoneCast reliably when enabled, remains optional, and meets a recorded ordinary-game false-activation target.
- Approved hotkeys/buttons work on native Steam Frame without suppressing unrelated game controls.
- Dashboard controls remain usable when gestures and hotkeys are disabled.
- Phone, settings, notification, and text-entry interactions are tested in portrait and landscape where applicable.
- Behavior is physically tested over at least one standalone VR scene, in both PhoneCast-first and game-first launch orders.
- Automated tests cover gesture state transitions, irregular timing, tracking loss, depth clamping, action routing, and input-release safety.

---

# Sprint 15 — Android Privacy Display Mode

**Status:** `[ ] Backlog`

## Objective

Reduce battery use and protect the locally visible phone while PhoneCast is
streaming, without dimming or obscuring the VR stream.

## Supported baseline

Implement an explicit, optional **Dim phone while casting** mode using public
Android APIs where device behavior permits it:

1. Save the user's brightness and adaptive-brightness state before changing it.
2. After casting starts, wait for a configurable idle delay and reduce physical
   display brightness to the lowest safe, usable level.
3. A physical screen interaction temporarily restores the prior brightness.
4. After the user-configurable delay, dim again while casting remains active.
5. Stopping casting, projection revocation, service failure, reboot recovery, or
   disabling the feature restores the prior settings on a best-effort basis.
6. Provide a persistent notification action and an obvious in-app recovery path
   that immediately restores brightness.

The feature must be separately opt-in and explain any required Android
**Modify system settings** access. It must not silently disable adaptive
brightness, leave the display unreadable after a crash, or log brightness and
interaction history unnecessarily.

Brightness is normally applied after display composition and therefore may dim
the physical panel without affecting MediaProjection output, but this must be
validated across supported Android versions and devices before approval.

## Privacy-screen investigation

Investigate a stronger **Privacy display** state that resembles a local lock or
black screen while leaving the captured stream and VR remote control available.
The ordinary Android lock screen is not acceptable: Android 15 QPR1 and newer
stop MediaProjection when the device locks, and that projection cannot be
resumed without a new capture session.

A normal application must not claim Samsung Phone Link-style local blanking
unless it is physically demonstrated through a supported API. Evaluate:

- whether a transparent or accessibility overlay can intercept physical touches
  without appearing in MediaProjection or blocking PhoneCast's injected remote
  gestures;
- whether public display/brightness APIs can provide a black-looking local panel
  while the encoded stream remains normal;
- OEM-supported APIs where available;
- an explicitly advanced ADB/Shizuku panel-power mode, kept separate from the
  consumer default and accompanied by reliable recovery controls.

A touch-blocking overlay cannot be assumed to pass the same touch through to the
underlying application, and injected Accessibility gestures may target that
same overlay. If physical-input blocking cannot coexist safely with VR remote
control, ship dimming only and document the limitation.

## Acceptance Criteria

- Dimming is opt-in and active only during an authorized casting session.
- The VR stream retains normal brightness and content while the physical panel
  is dimmed.
- Physical interaction restores the exact prior brightness behavior promptly,
  then the display dims again after the configured idle delay.
- Stop, projection revocation, permission removal, crash recovery, and device
  restart have tested restoration behavior and cannot strand the display at
  minimum brightness.
- Automatic and manual brightness modes are restored correctly.
- Locking the phone continues to follow Android privacy rules and is never
  represented as resumable projection.
- Any privacy-screen/touch-guard mode is separately opt-in, has an emergency
  escape, does not appear in the stream, and does not block VR-originated input.
- If those privacy-screen criteria cannot be met with supported APIs, the
  delivered feature is explicitly limited to safe brightness dimming.
- Physical validation covers the target phone, screen rotation, reconnect,
  notification shade use, casting stop/restart, and at least one failure path.

Detailed existing research and the validation matrix are in
[`docs/android-screen-off.md`](docs/android-screen-off.md).

---

# 6. Networking & Security

PhoneCast should assume that phone content is sensitive.

Do not expose the stream publicly.

Default behavior should be LAN-only.

Pairing should establish a trusted relationship between devices.

Eventually investigate:

- encrypted transport;
- device authentication;
- pairing codes;
- key persistence;
- revocation.

A random device on the LAN must not be able to request the phone screen.

Likewise, the Android sender must not accept arbitrary remote-control commands.

---

# 7. Performance Goals

These are targets rather than hard requirements.

### Video

Target:

```text
30 FPS
```

Stretch:

```text
60 FPS
```

### Latency

Optimize aggressively for perceived latency.

The system should feel suitable for:

- reading messages;
- changing music;
- checking Discord;
- interacting with ordinary Android apps.

It does NOT need latency appropriate for mobile gaming.

### VR Impact

PhoneCast must avoid causing:

- dropped VR frames;
- reprojection;
- stuttering;
- excessive GPU contention.

VR game performance takes priority over phone stream quality.

If resources become constrained:

```text
reduce phone FPS
        ↓
reduce resolution
        ↓
reduce bitrate
```

rather than impacting VR rendering.

---

# 8. Debug / Developer Overlay

Create an optional diagnostics view.

Example:

```text
PhoneCast Diagnostics

Stream: Connected
Resolution: 1080 × 2340
FPS: 29.8
Bitrate: 5.2 Mbps

Network RTT: 8 ms
Decode: 3.1 ms
Render: 0.8 ms

Dropped Frames: 2

Backend:
OpenVR / Windows x64
```

This will be extremely useful when moving from PC to Steam Frame.

---

# 9. Logging

Logging should be structured and useful.

Log:

- application startup;
- VR runtime detection;
- overlay creation;
- phone discovery;
- pairing;
- network connection;
- decoder initialization;
- resolution changes;
- orientation changes;
- disconnects;
- reconnects;
- major errors.

Avoid logging:

- notification contents;
- screen contents;
- typed text;
- authentication secrets.

---

# 10. Testing Strategy

Every subsystem should be testable independently.

Examples:

```text
FakeVideoSource
FakeNetworkTransport
FakeOverlayRenderer
FakePhoneInput
```

A generated test stream should allow VR development without needing the Android application running.

Likewise, Android streaming should be testable without VR.

Desired separation:

```text
Android test
      ↓
Desktop video window

VR test
      ↓
Generated texture

Networking test
      ↓
Generated video stream

Full integration
      ↓
Android → VR
```

This prevents every developer test from requiring the complete hardware stack.

---

# 11. Important Research Questions

Maintain these in `docs/steam-frame.md`.

### Highest Priority

Can a native ARM64 Steam Frame application create a persistent overlay over another standalone VR application?

**Current answer:** Native ARM64 overlay creation and visible coexistence with on-device standalone flat applications are confirmed. PhoneCast-first coexistence with standalone Cubism is now physically confirmed and corroborated by native OpenXR scene logs; the game-first launch order and broader lifecycle matrix remain pending. See `docs/steam-frame.md`.

### Additional Questions

- Does Steam Frame expose `IVROverlay` to standalone applications?
- Are overlay applications allowed to remain alive when another application launches?
- Is OpenVR preferable to OpenXR for this use case?
- Does OpenXR provide an appropriate composition-layer mechanism for persistent third-party overlays?
- What restrictions exist around compositor layers?
- Can controller events reach the overlay while another game has focus?
- What hardware video decoding APIs are available?
- Can the decoder output be transferred efficiently into the compositor?
- What background-process restrictions exist?
- Can PhoneCast auto-start?
- Can Android APK functionality on Frame assist with the implementation?
- Can local network discovery operate normally?
- What lifecycle behavior occurs when switching applications?

Never assume an answer.

Test where documentation is ambiguous.

---

# 12. Non-Goals for Initial MVP

Do NOT prioritize:

- iPhone support;
- cloud streaming;
- remote Internet streaming;
- account systems;
- social features;
- app store distribution;
- multiple simultaneous phones;
- fancy animations;
- elaborate themes;
- perfect UI;
- phone audio streaming;
- microphone forwarding.

These can be evaluated later.

The first goal is:

```text
PHONE SCREEN
     ↓
LOW LATENCY
     ↓
VR OVERLAY
     ↓
WHILE PLAYING A GAME
```

---

# 13. Definition of MVP

The MVP is complete when:

1. Android captures its display.
2. Android streams it over the local network.
3. Receiver decodes the stream.
4. Receiver displays it as a VR overlay.
5. Overlay remains visible while a VR game runs.
6. User can hide/show the overlay.
7. User can resize it.
8. User can reposition it.
9. Phone aspect ratio is preserved.
10. Performance remains acceptable during normal VR gameplay.

Remote phone control is NOT required for MVP.

Standalone Steam Frame operation is NOT required for the first MVP.

However, **nothing in the MVP architecture should unnecessarily prevent the standalone build.**

---

# 14. Definition of Standalone Success

The long-term milestone is:

```text
Android Phone
      │
      │ Wi-Fi
      ▼
Steam Frame
```

with:

```text
NO WINDOWS PC
NO DESKTOP SERVER
NO STREAMING PC DEPENDENCY
```

The user should be able to:

- put on Steam Frame;
- connect to their Android phone;
- launch a standalone VR game;
- reveal their phone;
- view it;
- interact with it;
- dismiss it;
- continue playing.

---

# 15. Coding Harness Instructions

Work one sprint at a time.

Before starting a sprint:

1. Read this entire document.
2. Review existing code.
3. Review previous sprint notes.
4. Research unknown APIs where necessary.
5. Create/update the sprint task checklist.
6. Identify architectural risks.

During implementation:

- Prefer simple code.
- Preserve platform boundaries.
- Avoid Windows-specific assumptions in Core.
- Do not implement speculative abstractions without a concrete use.
- Add tests for reusable logic.
- Keep documentation current.
- Build frequently.
- Commit logical milestones where repository access permits it.

After each sprint:

1. Build the complete project.
2. Run relevant tests.
3. Record what works.
4. Record what does not work.
5. Document known limitations.
6. Update architecture documentation if implementation differs from this design.
7. Update `design.md` sprint status.

## Windows-hosted Linux ARM64 validation

On the current Windows development machine, Linux ARM64 cross-builds run in Docker Desktop, not a separately installed Ubuntu/WSL distribution. A CMake cache created in the container records source paths under `/src` and must not be opened with Windows CMake; a path-mismatch error does not indicate an ARM64 source failure or broken WSL installation.

From Git Bash at the repository root, use a fresh container build directory and disable MSYS argument path conversion so Docker receives `/src` unchanged:

```bash
MSYS_NO_PATHCONV=1 docker run --rm \
  -v "$(pwd -W):/src" -w /src debian:12-slim sh -lc '
    apt-get update &&
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      cmake ninja-build g++-aarch64-linux-gnu git ca-certificates file libpulse-dev &&
    rm -rf out/build/linux-arm64-validation &&
    cmake -S . -B out/build/linux-arm64-validation -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64-gcc.cmake &&
    cmake --build out/build/linux-arm64-validation &&
    file out/build/linux-arm64-validation/bin/phonecast-receiver &&
    file out/build/linux-arm64-validation/bin/phonecast-vr-stream-receiver
  '
```

The expected artifact is an `ELF 64-bit ... ARM aarch64` executable. This validates compilation only; it does not validate Steam Frame deployment, compositor behavior, controller input, decoding, or physical interaction. Docker Desktop must be running. Reusing an existing `/src` cache is safe only from a container mounted at the same path.

Use:

```text
[ ] Not started
[~] In progress
[x] Complete
[!] Blocked
```

Do not mark functionality complete merely because it compiles.

It must meet its acceptance criteria.

---

# 16. First Task

**Status:** `[x] Completed and accepted as Sprint 0 foundational success`

The native ARM64 Hello Frame overlay was deployed to Steam Frame and visually confirmed over an on-device standalone 2D game. The stricter VR-scene coexistence test remains documented as a follow-up.

Historical task directive, now completed:

- Do **not** begin with the Android streaming application.
- Begin with `SPRINT 0 — HELLO FRAME`.
- Research current Valve Steam Frame/OpenVR/OpenXR documentation.
- Implement the smallest possible overlay experiment.
- Let the measured result guide the architecture.

The experiment answered the foundational question positively enough to proceed: a native ARM64 process can create a user-visible persistent OpenVR overlay on Steam Frame while an on-device standalone 2D application runs. The untested VR-scene case remains explicitly tracked.
