# PhoneCast VR

PhoneCast VR aims to show an Android phone as a persistent VR overlay, first through PC SteamVR and eventually directly on Steam Frame ARM64.

Moving to another development machine or starting a fresh Pi agent? Follow [`docs/new-machine-setup.md`](docs/new-machine-setup.md).

## Current status

- `[x]` Sprints 0–4 complete: Android capture, paired LAN streaming, low-latency decoding, and a responsive PC-hosted OpenVR overlay are physically approved
- `[x]` Sprint 5 placement complete for the PC-hosted path: head, world, left-controller, and right-controller modes with direct VR placement
- `[~]` Sprint 5.1 controller calibration is implemented and tested; physical ergonomic tuning remains
- `[~]` Sprint 6 glance mode is implemented and tested; joint physical review with Sprint 5.1 remains
- `[!]` Native coexistence over a standalone VR scene and the Steam Frame decoder/rendering backend remain future validation and Sprint 11 work
- `[!]` The current LAN transport is paired but not encrypted

## Sprint 4 VR streaming validation

For normal Windows use, double-click **`Start PhoneCast VR.cmd`** from the repository root. From the physical console it starts SteamVR when necessary and launches the receiver with pairing code `123456` on port `49321`. When run through Remote Desktop, it stops the RDP-bound Steam and VR processes, disconnects RDP, transfers that Windows session back to the physical console, waits for the physical display stack to settle, and restarts Steam, SteamVR, and PhoneCast there. Recovery and receiver logs are written under `out/logs`. Reconnecting through RDP can disturb VRLink again, so run the launcher before each new VR test. Double-click **`Stop PhoneCast VR.cmd`** to stop the receiver.

The equivalent manual command is:

```powershell
.\out\build\windows-x64\bin\phonecast-vr-stream-receiver.exe --pair-code 123456
```

Connect the Android sender to the PC as described below. The receiver starts hidden while continuing to decode. Short-click either thumbstick to cycle `Hidden -> Glance -> Expanded -> Pinned -> Hidden`. Hold a thumbstick for the configurable 0.6-second default to open the radial menu on that hand, release, point toward an item, and click to select. Its Settings wedge opens a controller-navigated in-headset configuration panel. The Glance preview follows the initiating hand; Expanded and Pinned restore normal placement. Hold **Ctrl+Alt** while pressing:

- `G` — cycle glance states
- `P` — quick Hidden/Expanded toggle
- `S` — open the in-headset settings panel
- `+` / `-` — scale
- arrow keys — move
- `Page Up` / `Page Down` — nearer/farther
- `]` / `[` — increase/decrease opacity
- `H` / `W` / `L` / `R` — select head/world/left-controller/right-controller placement
- `Home` — reset
- `End` — quit

The Windows OpenVR backend uses a reusable D3D11 texture on SteamVR's requested DXGI adapter. Media Foundation low-latency mode fixed sparse-screen buffering; portrait/landscape transitions, the right-edge padding fix, responsive static and dynamic content, and persistence over a PC SteamVR game are physically approved. Quantitative glass-to-glass measurement remains Sprint 10 work. See `docs/development.md` for placement calibration and `docs/glance-mode.md` for glance behavior and pending physical checks.

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
