# Development

## Receiver prerequisites

- Git
- CMake 3.24 or newer
- Ninja
- A C++17 compiler
- SteamVR for receiver runtime testing

CMake fetches the OpenVR 2.15.6 source pinned at commit `0924064316de3effbcd1acf1e309182a2deb1c05`.

## Windows x64 receiver

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir out/build/windows-x64 --output-on-failure
```

### Coding-harness Windows CMake note

The coding harness launches `bash`, where `cmake` is not on `PATH`, even though Windows CMake is installed at `C:\Program Files\CMake\bin`. Do not treat `bash: cmake: command not found` as a missing project prerequisite and do not guess another generator. Invoke the Windows tools through PowerShell:

```bash
powershell.exe -NoProfile -Command '& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-x64'
powershell.exe -NoProfile -Command '& "C:\Program Files\CMake\bin\cmake.exe" --build --preset windows-x64'
```

The preset uses the WinLibs MinGW compiler. When `ctest` is launched from the harness, another MinGW runtime earlier on `PATH` can make `phonecast-tests.exe` exit with Windows status `0xc0000139` (`STATUS_ENTRYPOINT_NOT_FOUND`). This is a runtime-DLL path problem, not a test failure. Prepend the exact compiler runtime directory recorded in `out/build/windows-x64/CMakeCache.txt` before running tests. On the current development machine:

```bash
powershell.exe -NoProfile -Command '$mingw="C:\Users\bangf\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"; $env:PATH="$mingw;$env:PATH"; Set-Location "C:\Projects\vr_mobile_overlay"; & "C:\Program Files\CMake\bin\ctest.exe" --test-dir out/build/windows-x64 --output-on-failure; exit $LASTEXITCODE'
```

Keep the outer Bash argument single-quoted so Bash does not expand PowerShell variables such as `$LASTEXITCODE`. If the compiler package changes, read `CMAKE_CXX_COMPILER` from `CMakeCache.txt` and use that compiler's adjacent `bin` directory rather than copying a stale path.

Run SteamVR, then:

```powershell
.\out\build\windows-x64\bin\phonecast-receiver.exe --duration-seconds 10
```

The build places `openvr_api.dll` and `phonecast-receiver.vrmanifest` beside the executable.

Sprint 3 desktop streaming receiver:

```powershell
.\out\build\windows-x64\bin\phonecast-stream-receiver.exe --pair-code 123456
```

Sprint 4 OpenVR streaming receiver (start SteamVR first):

```powershell
.\out\build\windows-x64\bin\phonecast-vr-stream-receiver.exe --pair-code 123456
```

For routine physical-console use, double-click `Start PhoneCast VR.cmd` in the repository root. It checks for an existing receiver, starts SteamVR if needed, launches the canonical `out/build/windows-x64` receiver on port `49321` with pairing code `123456`, and redirects logs to `out/logs`. It intentionally refuses to run in an RDP session because of the documented VRLink/D3D11 failure mode. Use `Stop PhoneCast VR.cmd` to stop the receiver.

The VR receiver starts in Sprint 6's **Hidden** state while continuing to decode video for immediate reveal. Short-click either thumbstick to cycle `Hidden -> Glance -> Expanded -> Pinned -> Hidden`; Glance uses a small preview on the initiating hand, while Expanded and Pinned restore normal placement. Hold either thumbstick click for the configurable 0.6-second default to open the radial menu on that hand, release, point the stick toward an item, and click to select; left View/right Menu cancels it. The Settings wedge opens the transactional in-headset settings panel: up/down selects, left/right adjusts, stick click activates, and View/Menu goes back or cancels. `Ctrl+Alt+G` performs the state cycle, `Ctrl+Alt+P` quickly toggles Hidden/Expanded, and `Ctrl+Alt+S` opens settings as a development fallback. See `docs/glance-mode.md` for the complete behavior and physical-test checklist.

Other global controls while **Ctrl+Alt** is held are: `+/-` changes scale, arrows move, `Page Up/Down` changes distance, `]/[` changes opacity, `H/W` selects head/world lock, `L/R` selects left/right-controller lock, `Home` resets, and `End` quits. In controller mode, scale, arrows, and distance modify only the selected hand's calibration. Point a SteamVR controller laser at the overlay and hold trigger to grab it directly; releasing creates a world-locked anchor at that pose.

Controller-locked placement can be calibrated without the keyboard. From another placement mode, press either controller's application-menu button to select that hand and enter calibration. Press the selected hand's menu button again to leave or re-enter calibration; pressing the other hand's menu button switches hands. While active:

- move the primary axis to adjust lateral position and height;
- hold grip and move the axis to adjust yaw and tilt;
- hold trigger and move the axis to adjust scale and distance;
- click the pad/primary-axis button to cycle controller-relative, face-user, world-upright, and wrist orientation;
- hold grip and click the pad to reset that hand.

Direct overlay grabbing is disabled while calibration mode is active. Changes are live and are persisted when the axis returns to rest or calibration mode exits. The original controller path polled legacy application-menu, grip, trigger, and primary-axis controls through `GetControllerState`; physical Steam Frame/VRLink testing found that neither thumbstick click reached the glance control. The receiver now identifies itself with its registered OpenVR application key and uses explicit SteamVR Input actions plus a shipped `frame_controller` binding. SteamVR has accepted and loaded that binding, but physical delivery from both controllers still requires retesting.

Overlay appearance and placement are saved by default to `%LOCALAPPDATA%\PhoneCastVR\overlay-settings.ini`. Use `--settings PATH` to select a different file. Version 1 settings are migrated on load; version 2 stores independent hand profiles. Selecting world lock snapshots the panel's current position into standing space. Selecting a controller requires that role to be available; an unavailable controller leaves the previous placement active. Temporary tracking loss retains the most recent valid pose and updates resume after recovery. Sprint 5's placement behavior was physically approved over a PC SteamVR game on 2026-10-02; Sprint 5.1's controls and defaults await physical approval.

The Windows receiver's overlay disappears when the user switches to a headset-native standalone game. That game is rendered by the headset's native compositor rather than the PC SteamVR compositor hosting this process, so this is expected for the current backend and does not block the PC-hosted Sprint 5 milestone. Native coexistence with a standalone VR scene still requires the Steam Frame ARM64 backend and validation tracked for Sprint 11.

The Android sender connects to the PC's LAN IPv4 address on TCP port `49321`. Windows Firewall must permit inbound private-network TCP traffic for the receiver. The pairing code must contain exactly six digits. The current protocol is not encrypted; use only a trusted development LAN.

### Important: do not start SteamVR through Windows RDP

A physical Steam Frame/VRLink test on 2026-10-02 showed that starting or restarting SteamVR from an active Remote Desktop session can break all streamed PC content, not only PhoneCast. Symptoms included a gray headset view, invisible raw and D3D11 overlays, games failing to appear, repeated VRLink `Failed to create d3d11textures` messages, and compositor `WaitForAcquire timed out` warnings.

Closing the RDP window is insufficient if Steam or SteamVR remains in the disconnected RDP session. Before testing:

1. run Steam and SteamVR in the physical Windows console session;
2. verify `query session` reports the development user on `console` rather than `rdp-tcp`;
3. reconnect or sleep/wake the Frame after correcting the Windows session;
4. confirm an ordinary PC game streams before diagnosing PhoneCast.

If the failure recurs, stop SteamVR, transfer/log into the physical console session, relaunch SteamVR there, reconnect or sleep/wake the Frame, and confirm ordinary game streaming first. Do not interpret overlay results obtained during the broken RDP/VRLink state as application results.

The 2026-10-02 physical Sprint 4 test subsequently confirmed a visible Android screen in the headset. On the multi-GPU host, OpenVR D3D11 overlays were invisible until the renderer used the adapter returned by `GetDXGIOutputInfo`; updates advanced only about once per five seconds until the D3D11 context was explicitly flushed. The corrected generated animation was visually smooth before the phone stream was retested successfully.

Current physical-test result: after enabling Media Foundation low-latency mode, the user reported near-instant initial appearance and touch-to-display response under one second, including isolated changes on static content; YouTube playback was watchable and the result was explicitly approved. These timings are user estimates rather than instrumented measurements. Landscape still fails while portrait works. The prior right-edge green bar is fixed. Persistence over a running VR game and the PC keyboard overlay controls are confirmed. Sprint 4 keyboard shortcuts adjust only the overlay; headset-native placement and interaction with Android are not implemented. Phone audio remains on the phone because audio transport is outside the visual MVP.

OpenVR's `VREvent_ProcessQuit` may refer to an unrelated scene application. It must not terminate the receiver; only explicit runtime/driver quit events should do so. Treating it as a shutdown request caused the overlay process to exit during the first VR-game transition test.

Useful overlay receiver options:

```text
--width N
--height N
--fps N
--overlay-width M
--distance M
--alpha A
--duration-seconds N
```

Use `--help` for ranges and defaults.

### Sparse-update latency regression (2026-10-02)

The Windows decoder now enables low-latency mode before configuring its media
types. Previously, its internal buffering could hold a screen update until more
frames arrived, despite millisecond decode times and an empty TCP receive queue.
See Microsoft's [H.264 decoder attributes](https://learn.microsoft.com/en-us/windows/win32/medfound/h-264-video-decoder)
and [low-latency property](https://learn.microsoft.com/en-us/windows/win32/medfound/codecapi-avlowlatencymode).

`phonecast-mf-sparse-frame-latency` tests an IDR and two P-frames with no future
input or EOS drain allowed before each picture appears. It fails before the fix
and passes afterward on the Windows development host. It needs Media Foundation,
not SteamVR, a phone, or FFmpeg. Fixture generation is documented in
`tests/fixtures/README.md`.

A full build and all nine tests passed in `out/build/windows-x64-latency` while
the old VR receiver remained running. To physically retest, exit the old receiver
(Ctrl+Alt+End), then run from the console session:

```powershell
.\out\build\windows-x64-latency\bin\phonecast-vr-stream-receiver.exe --pair-code 123456
```

No sender reinstall is required. Compare an isolated phone tap followed by no
motion, continuous scrolling, and startup; use the desktop receiver separately
for comparison. After restarting the fixed receiver, the user physically retested
and reported “this is perfect. very snappy.” A follow-up covered startup, isolated
updates on static content, and sustained dynamic content: startup was described as
near-instant, touch-to-display as under one second and near-instant, and YouTube as
watchable. The user explicitly approved the result. End-to-end responsiveness is
qualitatively confirmed; quantitative latency and startup timings remain unmeasured.
Sparse `rx-fps=0` on a static screen is not alone evidence of capture failure.

## Linux ARM64 receiver cross-build

Install an AArch64 GNU compiler, then configure with the checked-in toolchain:

```bash
cmake -S . -B out/build/linux-arm64 \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64-gcc.cmake
cmake --build out/build/linux-arm64
file out/build/linux-arm64/bin/phonecast-receiver
```

The result should identify as an AArch64 ELF. Deploy the executable and adjacent manifest with Steam Linux Runtime 3.0 ARM64 (Sniper). See `docs/steam-frame.md` for established Steam Frame deployment findings.

## Android sender

Requirements:

- Android SDK 36
- JDK 17 or newer
- An Android 10+ device for runtime validation

The sender includes a Gradle wrapper. From the repository root:

```powershell
cd android\sender
.\gradlew.bat testDebugUnitTest assembleDebug lintDebug
```

Install the debug APK on a connected device:

```powershell
adb install -r app\build\outputs\apk\debug\app-debug.apk
adb logcat -s PhoneCastCapture
```

See `android/sender/README.md` and `docs/android-capture.md` for behavior and permission requirements.

## CMake options

- `PHONECAST_BUILD_RECEIVER` — build the generated OpenVR receiver and, on Windows, the desktop and OpenVR stream receivers (default `ON`).
- `PHONECAST_BUILD_HELLO_FRAME` — retain the Sprint 0 experiment (default `ON`).
- `BUILD_TESTING` — build/register tests (default controlled by CTest, normally `ON`).

## Tests

Receiver unit tests use fake logging and overlay implementations, so they do not require SteamVR. Android configuration tests also run without a device. Neither suite replaces visual compositor checks or physical-device capture/encoder tests.
