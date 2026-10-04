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

# Sprint 10 — Performance Pass

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

Once standalone operation works, optimize PhoneCast specifically for Steam Frame.

Desired experience:

```text
Put on Frame

       ↓

Phone automatically discovered

       ↓

PhoneCast connects

       ↓

Launch VR game

       ↓

Play normally

       ↓

Open SteamVR dashboard

       ↓

Select PhoneCast Show / Glance / Pin

       ↓

Phone appears

       ↓

Interact

       ↓

Dismiss

       ↓

Continue playing
```

The user should not need to manually manage a server every session.

Investigate:

- automatic discovery;
- trusted-device pairing;
- reconnect;
- startup behavior;
- background operation;
- Steam Frame application lifecycle;
- OpenVR overlay autostart using `SetApplicationAutoLaunch`, verifying `GetApplicationAutoLaunch`, and starting a registered process with `LaunchDashboardOverlay` where appropriate;
- executable-bit and manifest-path validation in native packaging.

A third-party native Steam Frame utility demonstrates this autostart pattern, but PhoneCast must validate it with its own dashboard, networking, sleep/wake, and application transitions. Optional dashboard-closed hotkeys are not required for standalone UX.

---

# Sprint 13 — Optional Phone Audio

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

**Current answer:** Native ARM64 overlay creation and visible coexistence with an on-device standalone 2D application are confirmed. Coexistence with a standalone VR scene application remains untested. See `docs/steam-frame.md`.

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
      cmake ninja-build g++-aarch64-linux-gnu git ca-certificates file &&
    rm -rf out/build/linux-arm64-validation &&
    cmake -S . -B out/build/linux-arm64-validation -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64-gcc.cmake &&
    cmake --build out/build/linux-arm64-validation &&
    file out/build/linux-arm64-validation/bin/phonecast-receiver
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
