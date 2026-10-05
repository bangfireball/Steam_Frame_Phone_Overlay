# PhoneCast VR

PhoneCast VR mirrors an Android phone into VR as a low-latency OpenVR overlay. It can run through Windows SteamVR or directly on Steam Frame ARM64 without a PC.

> **v0.1 — personal alpha / near-release build**
>
> The core experience works and has been used on real hardware, but setup and parts of the interface are still rough. This is an early open-source release for technical users, not a polished consumer product.

## What works

- Android screen capture using MediaProjection and hardware H.264 encoding
- Paired local-network streaming at approximately 30 FPS
- Windows x64 receiver using Media Foundation, D3D11, and OpenVR
- Native Steam Frame ARM64 receiver using V4L2, Vulkan, and OpenVR
- Head-, world-, left-controller-, and right-controller-locked placement
- Show, hide, glance, pin, scale, opacity, distance, and placement controls
- SteamVR dashboard panel and in-headset settings
- Optional controller interaction with Android: tap, long-press, drag, swipe, scroll, and Back
- Optional notification cards with privacy filtering and app allow/block lists
- Portrait and landscape streaming on the established Windows path
- Persisted overlay and controller-placement settings
- Performance diagnostics and selectable Battery Saver, Standard, and Quality profiles

The native Steam Frame Vulkan path has been physically tested with smooth, flicker-free phone streaming and stable memory during a locally running flat game. Native coexistence with a true standalone VR scene, orientation recreation, clean shutdown, and longer lifecycle cases still need broader validation.

## Important limitations

- **The stream is not encrypted. Use PhoneCast only on a trusted private LAN.**
- Setup is currently manual: start the receiver, enter its LAN IP and six-digit pairing code on Android, and approve Android screen sharing.
- Native Steam Frame packaging, startup, reconnect, and sleep/wake behavior are not yet consumer-ready.
- The UI works but remains rough. Wrist-opening gestures and broad controller hotkeys are intentionally disabled because they were not reliable enough and could conflict with game input.
- With the dashboard closed, the persistent phone panel is view-only so PhoneCast does not steal controller input from the active game.
- Remote control requires explicit in-app consent and enabling the PhoneCast Android Accessibility service.
- Notification forwarding is optional and disabled by default. Notification text remains redacted unless separately enabled.
- Android may stop MediaProjection when the phone locks. Start a new casting session after unlocking.
- Phone audio is not currently streamed.
- Steam Frame support targets current development hardware/runtime behavior and may be affected by SteamOS or SteamVR updates.

See [`design.md`](design.md) for exact validation status and unresolved hardware checks. Compilation alone is not treated as physical validation.

## Requirements

### Android sender

- Android 10 or newer
- Android SDK 36 and JDK 17 to build
- A device with an H.264/AVC surface-input encoder

### Windows receiver

- Windows x64
- SteamVR
- CMake 3.24 or newer, Ninja, and a C++17 compiler

### Native Steam Frame receiver

- Steam Frame with native SteamVR/OpenVR support
- Linux ARM64 cross-build environment described in [`docs/development.md`](docs/development.md)
- Steam Linux Runtime 3.0 ARM64 (Sniper) for deployment

## Build the Android app

From the repository root:

```powershell
cd android\sender
.\gradlew.bat testDebugUnitTest assembleDebug lintDebug
```

Install the debug APK:

```powershell
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

See [`android/sender/README.md`](android/sender/README.md) for permissions, optional remote control, notifications, and diagnostics.

## Build the Windows receiver

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir out/build/windows-x64 --output-on-failure
```

The coding harness on the original development machine needs the PowerShell and MinGW runtime-path commands documented in [`docs/development.md`](docs/development.md).

## Run on Windows

Do not start or restart SteamVR through Windows Remote Desktop. Doing so can break VRLink texture creation and produce a gray or missing stream. Follow the physical-console procedure in [`docs/development.md`](docs/development.md).

For normal use, run:

```text
Start PhoneCast VR.cmd
```

The launcher starts the receiver on TCP port `49321` with pairing code `123456`. The equivalent manual command is:

```powershell
.\out\build\windows-x64\bin\phonecast-vr-stream-receiver.exe --pair-code 123456
```

On Android:

1. Enter the receiver computer's private LAN IPv4 address.
2. Enter the same six-digit pairing code.
3. Select a streaming profile; **Standard** is the validated default.
4. Press **Start casting** and approve Android's screen-sharing prompt.
5. Open the SteamVR dashboard and select the **PhoneCast** tab.
6. Use Show/Hide, Glance, Pin, Settings, and placement controls with the controller laser.

Use `Stop PhoneCast VR.cmd` to stop the Windows receiver.

## Run directly on Steam Frame

Cross-build the ARM64 receiver with the Docker command in [`docs/development.md`](docs/development.md). Deploy `phonecast-vr-stream-receiver` and its adjacent manifest/input JSON files, preserve the executable bit, and launch:

```bash
chmod +x phonecast-vr-stream-receiver
./phonecast-vr-stream-receiver --pair-code 123456
```

Then connect Android to the Steam Frame's private LAN address instead of the PC address. The receiver automatically searches for the native stateful H.264 decoder; use `--video-device /dev/videoN` only if discovery selects incorrectly.

Native installation is currently developer-oriented. Read [`platform/steam-frame-arm64/README.md`](platform/steam-frame-arm64/README.md) before deploying.

## Optional Android remote control

Casting does not require remote control. To enable it:

1. Read the disclosure in the Android app.
2. Enable **Allow remote control while casting**.
3. Open Android Accessibility settings.
4. Explicitly enable **PhoneCast remote control**.

The service injects only receiver-requested gestures and Back. It is configured not to retrieve window content. Disable either gate to stop remote input. See [`docs/remote-control.md`](docs/remote-control.md) for security and Google Play policy considerations.

## Optional notifications

Notification forwarding is separately opt-in. Android provides package allow/block lists, and sensitive title/body text is excluded unless explicitly enabled. Notifications travel over the same currently unencrypted trusted-LAN connection.

See [`docs/notifications.md`](docs/notifications.md) for the complete privacy model.

## Development status

PhoneCast v0.1 is functional but actively developed. Completed work includes Android capture, streaming, PC and native decoding, VR rendering, placement, remote interaction, notifications, and performance measurement. Current work is focused on finishing native Steam Frame lifecycle/VR-scene validation, followed by standalone startup/reconnect UX and later interface/gesture improvements.

Useful documentation:

- [`design.md`](design.md) — roadmap, sprint status, acceptance criteria, and physical evidence
- [`docs/architecture.md`](docs/architecture.md) — Core/VR/platform boundaries
- [`docs/development.md`](docs/development.md) — complete build, test, run, and deployment instructions
- [`docs/protocol.md`](docs/protocol.md) — network protocol and security limitations
- [`docs/performance.md`](docs/performance.md) — measured performance and test procedure
- [`docs/steam-frame.md`](docs/steam-frame.md) — native Steam Frame findings and unresolved questions
- [`docs/glance-mode.md`](docs/glance-mode.md) — dashboard and presentation behavior
- [`docs/remote-control.md`](docs/remote-control.md) — Android interaction design and permissions
- [`docs/notifications.md`](docs/notifications.md) — notification privacy and behavior

## Agentic development disclosure

PhoneCast VR has been developed with substantial assistance from AI coding agents. Agents have been used for research, planning, implementation, refactoring, test creation, debugging, and documentation.

The project owner directs the work, chooses product behavior, reviews results, and performs the physical Android, SteamVR, and Steam Frame validation that an agent cannot perform by compiling code. The repository deliberately distinguishes automated/build evidence from physically observed behavior.

AI assistance does not guarantee correctness or security. Review the source, permissions, protocol limitations, and platform-specific code before relying on it or distributing a build.

## Contributing

Issues and focused pull requests are welcome. Please preserve the separation between portable Core/VR logic and Windows or Steam Frame platform code. Hardware-dependent claims should include the tested device, runtime, launch order, and whether the result was physically observed.

Do not commit credentials, `.env` files, device-specific secrets, Android `local.properties`, build output, or authentication configuration.

## License

PhoneCast VR is open-source software licensed under the [MIT License](LICENSE).
