# PhoneCast VR

PhoneCast VR aims to show an Android phone as a persistent VR overlay, first through PC SteamVR and eventually directly on Steam Frame ARM64.

## Current status

- `[x]` Sprint 0 foundational native-overlay feasibility accepted
- `[x]` Sprint 1 reusable receiver architecture implemented
- `[x]` Windows x64 clean build and automated tests
- `[x]` Windows OpenVR runtime accepted continuous generated-frame submissions
- `[x]` Linux ARM64 cross-build produced an AArch64 receiver
- `[x]` Sprint 1 animated texture visually confirmed on Steam Frame
- `[!]` Continuous `SetOverlayRaw` uploads visibly flicker; this prototype path is not the final streaming renderer
- `[!]` Follow-up: validate coexistence with a standalone VR scene application
- `[x]` Sprint 2 Android sender complete and validated on a physical device
- `[x]` Android debug APK builds, unit tests pass, and lint passes
- `[~]` Sprint 3 phone-to-PC streaming visually confirmed on a physical device
- `[!]` Sprint 3 follow-up: physically validate the decoder green-edge fix and improve visibly stuttering playback
- `[~]` Sprint 4 Android screen is physically visible in the PC-streamed SteamVR headset
- `[x]` Phone stream receiver feeds a reusable D3D11-backed OpenVR texture on SteamVR's selected DXGI adapter
- `[x]` Explicit D3D11 flushing produces visually smooth generated-overlay animation
- `[x]` Global PC keyboard controls cover show/hide, scale, movement, distance, opacity, and reset
- `[x]` Physical validation confirms portrait display and removal of the right-edge green bar
- `[!]` Landscape currently fails; startup takes roughly 10–30 seconds; frame pacing is poor and latency is unusable
- `[x]` Phone overlay persists over a running VR game and PC keyboard controls are validated
- `[~]` Instrumentation found the phone actually sending roughly 90–110 FPS; an encoder-side 30 FPS cap, burst tolerance, startup-stage timing, and an immediate waiting overlay are implemented for physical validation
- `[!]` Follow-up: validate those changes, fix landscape orientation, and quantify glass-to-glass latency

## Sprint 4 VR streaming validation

Start SteamVR, then run the VR stream receiver with a six-digit code:

```powershell
.\out\build\windows-x64\bin\phonecast-vr-stream-receiver.exe --pair-code 123456
```

Connect the Android sender to the PC as described below. The receiver keeps the overlay head-relative and preserves the decoded phone aspect ratio. Hold **Ctrl+Alt** while pressing:

- `P` — show/hide
- `+` / `-` — scale
- arrow keys — move
- `Page Up` / `Page Down` — nearer/farther
- `]` / `[` — increase/decrease opacity
- `Home` — reset
- `End` — quit

The Windows OpenVR backend uses a reusable D3D11 texture rather than continuous `SetOverlayRaw` uploads. It selects SteamVR's requested DXGI adapter and flushes each update; both were required for a visible, smoothly updating VRLink overlay on the validation PC. Physical Android-to-headset picture, persistence over a running VR game, and PC keyboard controls are confirmed. Landscape and streaming performance still prevent Sprint 4 from being complete.

The latest instrumented physical test measured about 15.4 seconds from connection to first submitted phone frame. It also showed roughly 90–110 received FPS despite the requested 30 FPS, about 5–6 ms decode time, roughly 0.5 ms steady-state D3D submission, repeated resynchronization, and only about 8–16 rendered updates per second. The next build applies MediaCodec's encoder-side maximum-FPS control, increases tolerance for short transport bursts, reports configuration/keyframe/submission startup stages separately, and shows a waiting texture immediately so hotkeys work before video arrives. These changes require physical validation. Portrait works and the prior green edge is fixed, but landscape currently fails. Sprint 4's PC keyboard shortcuts control the overlay itself only; they do not interact with Android. Headset-native placement and phone control are later placement/interaction work. Phone audio is not part of the visual MVP and is tracked as an optional later sprint.

## Sprint 3 streaming validation

Start the Windows desktop receiver with a six-digit code:

```powershell
.\out\build\windows-x64\bin\phonecast-stream-receiver.exe --pair-code 123456
```

Allow TCP port `49321` through Windows Firewall. In the Android app, enter the PC's LAN IPv4 address and the same code, then press **Start casting**. The desktop window reports connection state, decoded FPS, bitrate, and dropped frames.

The current transport is LAN-only framed TCP and is **not encrypted**. See [`docs/protocol.md`](docs/protocol.md) before testing.

## Sprint 2 Android sender

The Android app requests MediaProjection consent, captures into a surface-input H.264 encoder at 30 FPS, drains encoded frames, reports diagnostics, and handles start/stop and capture resizing. It intentionally does not transmit data until Sprint 3.

```powershell
cd android\sender
.\gradlew.bat testDebugUnitTest assembleDebug lintDebug
```

Debug APK:

```text
android/sender/app/build/outputs/apk/debug/app-debug.apk
```

Physical-device validation confirmed permission handling, increasing encoded output counters, orientation changes, and stop/restart behavior.

## Sprint 1 receiver

The receiver continuously generates an animated RGBA test texture and submits it through a platform-neutral overlay interface to the OpenVR backend.

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir out/build/windows-x64 --output-on-failure
.\out\build\windows-x64\bin\phonecast-receiver.exe --duration-seconds 10
```

See:

- [`design.md`](design.md) — product and sprint plan
- [`docs/architecture.md`](docs/architecture.md) — production boundaries and interfaces
- [`docs/android-capture.md`](docs/android-capture.md) — Android capture decisions and validation status
- [`docs/protocol.md`](docs/protocol.md) — Sprint 3 transport, framing, and security limitations
- [`android/sender/README.md`](android/sender/README.md) — sender build and test instructions
- [`docs/development.md`](docs/development.md) — build, test, and run instructions
- [`docs/steam-frame.md`](docs/steam-frame.md) — Steam Frame evidence and open questions
- [`experiments/hello-frame/README.md`](experiments/hello-frame/README.md) — Sprint 0 experiment
