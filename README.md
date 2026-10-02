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
- `[~]` Sprint 3 phone-to-PC streaming implemented; physical-device validation pending

## Sprint 3 streaming validation

Start the Windows desktop receiver with a six-digit code:

```powershell
.\out\build\windows-x64\bin\phonecast-stream-receiver.exe --pair-code 123456
```

Allow TCP port `47990` through Windows Firewall. In the Android app, enter the PC's LAN IPv4 address and the same code, then press **Start casting**. The desktop window reports connection state, decoded FPS, bitrate, and dropped frames.

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
