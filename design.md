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

**Status:** `[~] In progress — end-to-end picture validated; performance and landscape remain unresolved`

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
- `[!]` Approximately 30 FPS LAN validation — current preview is visibly stuttering
- `[x]` Receiver restart/reconnection validation
- `[~]` Orientation validation — portrait works end to end; landscape currently fails
- `[!]` Latency — decoder measures roughly 5–6 ms, but observed end-to-end latency is currently unusable and glass-to-glass latency is not quantified
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

**Status:** `[~] In progress — first visual MVP path works over a VR game; performance and landscape block completion`

- `[x]` Shared paired TCP video server used by desktop and VR receivers
- `[x]` Windows Media Foundation decoder connected to the OpenVR overlay
- `[x]` Reusable D3D11 texture submission through `SetOverlayTexture` on Windows
- `[~]` Aspect-preserving dimension updates implemented; portrait works but landscape fails physically
- `[x]` Global show/hide, scale, distance, opacity, move, and reset controls
- `[x]` Decoder visible-versus-coded dimension handling removes the green edge
- `[x]` Windows clean build, automated tests, and generated-texture OpenVR runtime test
- `[x]` Physical Android stream visible in the SteamVR headset
- `[x]` D3D11 overlay uses SteamVR's DXGI adapter and explicitly flushes updates; generated animation is visually smooth
- `[x]` Phone overlay remains visible over a running VR game
- `[x]` Physical validation of PC keyboard overlay controls
- `[~]` Orientation validation: portrait works; landscape currently fails
- `[x]` Physical validation confirms the right-edge green bar is fixed
- `[!]` Phone-stream frame pacing is poor and observed latency is currently unusable
- `[~]` Initial overlay appearance takes approximately 10–30 seconds; measure codec-configuration/keyframe wait and reduce startup delay
- `[ ]` Record quantitative sustained FPS and glass-to-glass latency measurements
- `[x]` Overlay lifecycle ignores unrelated `VREvent_ProcessQuit` events during VR scene transitions
- `[!]` Test-environment hazard: starting SteamVR under Windows RDP can break VRLink D3D11 texture creation and produce a gray stream; test only from the physical console session (documented in `docs/development.md`)

## Session Handoff Snapshot — 2026-10-02

Last validated commits:

- `9bb524f` — Sprint 4 Android-to-VR implementation;
- `63cdab3` — physical headset findings recorded;
- `5169822` — overlay survives VR scene transitions.

Physically confirmed on Windows PC → VRLink → Steam Frame:

- Android portrait screen appears as a head-relative OpenVR overlay;
- overlay remains visible over a running VR game;
- PC keyboard controls work;
- generated D3D11 animation is smooth after selecting OpenVR's DXGI adapter and flushing updates;
- the right-edge green bar is fixed.

Current blockers, in priority order:

1. Fix poor phone-stream frame pacing and unusable perceived latency.
2. Reduce the approximately 10–30 second first-frame delay.
3. Fix landscape reconfiguration.
4. Record sustained FPS, queue/drop behavior, and quantitative glass-to-glass latency.

Useful evidence for the next session:

- Media Foundation decode itself is roughly 5–6 ms, so it does not explain the full latency.
- During the successful game test, diagnostics accumulated thousands of receiver-queue drops while decoded FPS rose toward roughly 48, suggesting producer/consumer pacing and queue policy need investigation.
- The current Windows path still performs NV12 → CPU RGBA → D3D11 upload.
- Do not test through RDP; confirm `query session` shows the user on `console` before launching SteamVR.
- Sprint 4 keyboard controls adjust only the overlay. Phone input is Sprint 7/8, and optional phone audio is Sprint 13.

Recommended next work:

1. physically validate the newly implemented one-second receive/decode/render, queue-age, drop, resync, and first-frame diagnostics;
2. verify whether the Android fixed-30-FPS surface request and low-latency/no-B-frame encoder hints correct output cadence on the test phone;
3. validate the new queue-overrun/sequence-gap policy, which discards dependent frames and requests a fresh keyframe instead of decoding arbitrary inter-frame gaps;
4. continue inspecting Media Foundation output draining if physical pacing remains poor;
5. retest startup, landscape reconfiguration, and steady-state pacing before beginning Sprint 5.

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

### Acceptance Criteria

User can switch between:

- World
- Head
- Left controller
- Right controller

Allow direct VR placement adjustment (for example, grab/move or equivalent controller controls) so repositioning does not require the PC keyboard.

Settings persist between sessions.

---

# Sprint 6 — Glance Mode

## Objective

Prevent the phone from constantly cluttering the user's VR view.

Introduce:

```text
Hidden
   ↓
Glance
   ↓
Expanded
   ↓
Pinned
```

A wrist/controller gesture or configurable action should reveal the phone.

Example:

```text
Normal gameplay

       ↓

Raise wrist

       ↓

Small phone preview

       ↓

Select

       ↓

Full phone overlay
```

Investigate reliable gesture detection before implementing complex gesture logic.

Provide a button-based fallback.

---

# Sprint 7 — VR Interaction

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

Raise wrist / press shortcut

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
- Steam Frame application lifecycle.

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
