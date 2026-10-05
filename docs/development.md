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

For routine review testing, double-click `Start PhoneCast VR.cmd` in the repository root. At the physical console it checks process session ownership, starts SteamVR if needed, replaces a receiver running from an older build path, launches the current validated `out/build/windows-x64` receiver on port `49321` with pairing code `123456`, and redirects logs to `out/logs`. If launched through RDP, it stops PhoneCast, SteamVR, and the Steam client; starts a detached recovery helper; requests an elevated `tscon` transfer of the current Windows session to the physical console; waits ten seconds for the physical display stack to settle; and restarts Steam, SteamVR, and PhoneCast only after the helper confirms the console attachment. The RDP connection closes intentionally. Recovery logs are written as `out/logs/console-recovery-*.log`. Use `Stop PhoneCast VR.cmd` to stop the receiver.

The VR receiver starts in Sprint 6's **Hidden** state while continuing to decode video for immediate reveal. On startup the OpenVR backend adds a **PhoneCast** icon/tab to the SteamVR dashboard. Open the dashboard with the normal system button, select PhoneCast, and use its large Show/Hide, Glance, Pin, Settings, Head, World, Left, and Right laser targets. The dashboard icon, panel, and pointer controls are physically approved on the Windows/VRLink path. Dashboard creation remains non-fatal, and native ARM64 lifecycle validation is still pending. Quick Access injection and receiver autostart are not implemented.

The dashboard header also reports Android remote-control readiness. It distinguishes a closed in-app consent gate, a disconnected Accessibility service, both gates closed, and ready state; the receiver console logs the same warning without logging input details. Gate status is available only after an authenticated phone connection.

The dashboard is the supported in-headset entry point. Experimental wrist-pose and thumbstick long-press menu gestures are disabled at runtime. The broad overlay-global SteamVR Input action set is also not initialized or submitted, so ordinary operation does not claim game buttons/axes and does not require SteamVR's experimental overlay input overrides. The dormant action and gesture implementation remains only for controlled future investigation. The Settings target opens a styled portrait transactional panel controlled by the same laser: click rows/actions, drag numeric sliders, or use the visible `-` and `+` targets for precise adjustment. Left/Right Dock categories correspond to the dashboard's docking controls. The panel is world-locked when opened; trigger-drag its bottom **Move** handle and release to place the menu without changing phone placement. The dashboard's **Back** target and the translucent `<` button at the phone overlay's lower-left edge send Android Back when optional remote control is enabled. The small `×` at the lower-right hides the phone locally without changing Android. `Ctrl+Alt+G` performs the state cycle, `Ctrl+Alt+P` quickly toggles Hidden/Expanded, and `Ctrl+Alt+S` opens settings as a development fallback. See `docs/glance-mode.md` and `docs/remote-control.md` for complete behavior and deferred physical-test items.

Other global controls while **Ctrl+Alt** is held are: `+/-` changes scale, arrows move, `Page Up/Down` changes distance, `]/[` changes opacity, `H/W` selects head/world lock, `L/R` selects left/right-controller lock, `Home` resets, and `End` quits. In controller mode, scale, arrows, and distance modify only the selected hand's calibration. Open the normal SteamVR dashboard, then point its controller laser at the phone overlay and use trigger for phone taps, swipes, and live held-trigger drag scrolling. To reposition PhoneCast, point at the horizontal handle below the phone, hold trigger, move, and release; this creates a world-locked anchor at that pose without relying on the controller grip button. With the dashboard closed, the persistent phone is deliberately view-only: it does not request OpenVR's system-wide laser mode or take controller poses/input away from the running game. Physical retesting confirmed that the phone remains visible and updates during game play, including video playback; game hands remain active; dashboard-open interaction works; and closing the dashboard immediately restores priority to the game. Sprint 7–8 interaction requires both Android gates—the in-app **Allow remote control while casting** checkbox and the Accessibility service. Controller/hand-locked placement may visibly jitter while swept across the view; this is accepted as a non-blocking follow-up.

Controller-locked placement can be calibrated without the keyboard. From another placement mode, press either controller's application-menu button to select that hand and enter calibration. Press the selected hand's menu button again to leave or re-enter calibration; pressing the other hand's menu button switches hands. While active:

- move the primary axis to adjust lateral position and height;
- hold grip and move the axis to adjust yaw and tilt;
- hold trigger and move the axis to adjust scale and distance;
- click the pad/primary-axis button to cycle controller-relative, face-user, world-upright, and wrist orientation;
- hold grip and click the pad to reset that hand.

Direct overlay grabbing is disabled while calibration mode is active. Changes are live and are persisted when the axis returns to rest or calibration mode exits. Earlier experiments replaced legacy controller polling with explicit SteamVR Input actions and a shipped `frame_controller` binding; SteamVR accepted the binding but did not deliver commands in the physical Windows/VRLink test. Research subsequently established that an active overlay-global set can suppress game bindings and still cannot override dashboard focus. PhoneCast therefore leaves that explicit global set dormant. Dashboard settings are the supported calibration route; no retest with experimental overlay input overrides is required for ordinary use.

Overlay appearance and placement are saved by default to `%LOCALAPPDATA%\PhoneCastVR\overlay-settings.ini`. Use `--settings PATH` to select a different file. Version 1 settings are migrated on load; version 2 adds independent hand profiles, and version 3 adds notification-card size and placement. Selecting world lock snapshots the panel's current position into standing space. Selecting a controller requires that role to be available; an unavailable controller leaves the previous placement active. Temporary tracking loss retains the most recent valid pose and updates resume after recovery. Sprint 5's placement behavior was physically approved over a PC SteamVR game on 2026-10-02. Sprint 5.1's configurable implementation is closed; extended tuning of controller defaults and ergonomics is explicitly deferred to ongoing headset use.

The Windows receiver's overlay disappears when the user switches to a headset-native standalone game. That game is rendered by the headset's native compositor rather than the PC SteamVR compositor hosting this process, so this is expected for the current backend and does not block the PC-hosted Sprint 5 milestone. Native coexistence with a standalone VR scene still requires the Steam Frame ARM64 backend and validation tracked for Sprint 11.

The Android sender connects to the PC's LAN IPv4 address on TCP port `49321`. Windows Firewall must permit inbound private-network TCP traffic for the receiver. The pairing code must contain exactly six digits. The current protocol is not encrypted; use only a trusted development LAN.

Android Settings → Streaming quality selects a persisted profile for the next capture session: Battery Saver (960-pixel long edge/20 FPS), Standard (1280/30), or Quality (1920/30). Standard retains the physically accepted baseline. The other profiles remain measurement candidates until Sprint 10 physical validation; Quality may affect game performance.

The Android app's main screen retains the receiver address and pairing code for a short repeat-start flow. Remote-control and notification options are under the menu button. If Android revokes projection (including supported screen-lock behavior), returning to the app refreshes the action to **Start casting** automatically instead of requiring a manual Stop first.

### Important: do not start SteamVR through Windows RDP

A physical Steam Frame/VRLink test on 2026-10-02 showed that starting or restarting SteamVR from an active Remote Desktop session can break all streamed PC content, not only PhoneCast. Symptoms included a gray headset view, invisible raw and D3D11 overlays, games failing to appear, repeated VRLink `Failed to create d3d11textures` messages, and compositor `WaitForAcquire timed out` warnings.

Closing the RDP window is insufficient if Steam or SteamVR remains in the disconnected RDP session. Before testing:

1. run `Start PhoneCast VR.cmd`; when invoked through RDP, approve its Windows elevation prompt and expect RDP to disconnect;
2. wait for the detached helper to transfer the session and restart SteamVR and PhoneCast at the physical console;
3. reconnect or sleep/wake the Frame after correcting the Windows session;
4. confirm an ordinary PC game streams before diagnosing PhoneCast.

You may reconnect through RDP between tests, but doing so can move the desktop away from the console and invalidate VRLink again. Run `Start PhoneCast VR.cmd` before each subsequent VR test; it repeats the stop, transfer, and clean restart. Do not interpret overlay results obtained during the broken RDP/VRLink state as application results. If recovery fails, inspect the newest `out/logs/console-recovery-*.stderr.log` and use `query session` to verify that the user session actually became `console`.

The 2026-10-02 physical Sprint 4 test subsequently confirmed a visible Android screen in the headset. On the multi-GPU host, OpenVR D3D11 overlays were invisible until the renderer used the adapter returned by `GetDXGIOutputInfo`; updates advanced only about once per five seconds until the D3D11 context was explicitly flushed. The corrected generated animation was visually smooth before the phone stream was retested successfully.

Current physical-test result: after enabling Media Foundation low-latency mode, the user reported near-instant initial appearance and touch-to-display response under one second, including isolated changes on static content; YouTube playback was watchable and the result was explicitly approved. These timings are user estimates rather than instrumented measurements. Landscape still fails while portrait works. The prior right-edge green bar is fixed. Persistence over a running VR game and the PC keyboard overlay controls are confirmed. Sprint 4 keyboard shortcuts adjust only the overlay; headset-native placement and interaction with Android are not implemented. Phone audio remains on the phone because audio transport is outside the visual MVP.

OpenVR's `VREvent_ProcessQuit` may refer to an unrelated scene application. It must not terminate the receiver; only explicit runtime/driver quit events should do so. Treating it as a shutdown request caused the overlay process to exit during the first VR-game transition test.

Useful generated-overlay receiver options:

```text
--width N
--height N
--fps N
--overlay-width M
--distance M
--alpha A
--duration-seconds N
```

The Windows VR stream receiver additionally accepts `--performance-log PATH`. It writes a flushed one-second CSV containing startup, stream, queue, process CPU/memory, and OpenVR compositor frame-timing samples. The 2026-10-04 Standard-profile physical run was owner-approved with first submission at 381.8 ms, 29.93 dynamic receive/decode FPS, 2.00 Mbps average bitrate, 1.13% average process CPU, approximately 69 MiB working set, and zero OpenVR-reported dropped/mispresented frames. See [`performance.md`](performance.md) for full evidence, limitations, and the glass-to-glass procedure. Use `--help` for current options.

### RDP / VRLink diagnostic comparison (2026-10-03)

The paused investigation, evidence chronology, and future test matrix are recorded in [`docs/rdp-vrlink-recovery.md`](rdp-vrlink-recovery.md).

Recovery has physically transferred the user to the console and started Steam,
SteamVR, and PhoneCast, but sustained VRLink stability is **not validated**.
The latest run began streaming around 14:15:35, requested video resets at
14:15:49, and lost its connection before the logged RDP reconnect at 14:19:19.
That reconnect therefore does not explain the initial freeze. Windows events do
not establish whether a reconnect was manual or automatic. PhoneCast authenticated
and submitted phone frames (first frames at 549 ms and 192 ms on two connections),
but remained visually unconfirmed; its default Hidden startup and unconfirmed
controller input must be isolated from the headset-stream fault.

Two root-level diagnostic launchers use the same RDP recovery:

1. `Test VR - No PhoneCast.cmd`: stops any existing PhoneCast receiver and starts
   SteamVR only. Keep Android casting off and test ordinary PC streaming for at
   least five minutes.
2. `Test VR - Visible PhoneCast.cmd`: starts the diagnostic receiver with
   `--diagnostic-visible`. It ignores saved appearance/placement, uses default
   centered head-locked placement, and starts Pinned/visible with the waiting
   texture before Android connects. No controller action is needed. Settings are
   not read or written in this mode; controls can still change the live session.
   First check the waiting panel, then cast from Android and check live video.

Both tests record Windows session rows and process/session IDs every second for
10 minutes in `out/logs/session-<mode>-<timestamp>.jsonl`. This is observational:
no reconnect-blocking policy or automatic repair loop is installed. An RDP
reconnect during startup aborts subsequent launches when detected; reconnecting
later can still disturb a running VR session. Visible-mode receiver logs are
separate timestamped `out/logs/visible-*.log` files. Note the approximate time of
any freeze so it can be correlated with these records and SteamVR logs.

The visible launcher uses `out/build/windows-x64-diagnostic` to avoid replacing
the currently running canonical executable. Build with `cmake --preset
windows-x64 -B out/build/windows-x64-diagnostic`, then `cmake --build
out/build/windows-x64-diagnostic` (use the compiler/Ninja PATH guidance above).
Normal `Start PhoneCast VR.cmd` behavior remains unchanged. A full diagnostic
build and all 11 CTest tests passed; PowerShell syntax and the read-only session
monitor smoke test passed. These do not validate headset visibility or stability.

On the **RDP client computer**, disable automatic reconnect for the test. In
classic `mstsc`, uncheck **Experience → Reconnect if the connection is dropped**,
or set `autoreconnection enabled:i:0` in the saved `.rdp` connection. Close any
retry dialog after recovery disconnects. No client settings have been changed by
PhoneCast. See [Microsoft RDP properties](https://learn.microsoft.com/en-us/azure/virtual-desktop/rdp-properties).
Save game progress before either test: recovery restarts Steam/SteamVR and may
force their shutdown; console transfer can leave the physical desktop unlocked.

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

The result should identify as an AArch64 ELF. Sprint 11 also builds the native streaming receiver:

```bash
file out/build/linux-arm64/bin/phonecast-vr-stream-receiver
```

Deploy `phonecast-vr-stream-receiver` and its adjacent manifest/input JSON files with Steam Linux Runtime 3.0 ARM64 (Sniper), preserve the executable bit, and launch it on Steam Frame with:

```bash
./phonecast-vr-stream-receiver --pair-code 123456
```

The native receiver automatically searches `/dev/video0` through `/dev/video63` for a stateful streaming H.264 V4L2 M2M decoder. Use `--video-device /dev/videoN` to select one explicitly and `--performance-log PATH` for the same one-second receiver/OpenVR CSV diagnostics used by the Windows path. Settings default to the XDG configuration directory. `SIGINT` and `SIGTERM` request clean shutdown.

This cross-build validates compilation only. Qualcomm/iris V4L2 decoding, visible output, and the Vulkan `SetOverlayTexture` renderer are physically established on Steam Frame. Linux converts decoded NV12 to CPU RGBA, uploads through a persistently mapped Vulkan staging buffer, and submits reusable double-buffered images. The Vulkan loader is opened at runtime from `libvulkan.so.1`, so no cross-build Vulkan development package is required. The measured local-game run was project-owner approved as smooth and flicker-free with stable memory; orientation changes, clean shutdown, longer lifecycle cases, and actual standalone-VR-scene coexistence remain pending. Do not describe dma-buf/zero-copy sharing as implemented. See `platform/steam-frame-arm64/README.md` and `docs/steam-frame.md`.

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

For optional VR remote control, first read the disclosure in the sender, check **Allow remote control while casting**, choose **Enable PhoneCast in Accessibility settings**, and explicitly enable **PhoneCast remote control**. Screen casting works without this service. The service injects only receiver-requested gestures and Back; it does not retrieve window content. Disable either the in-app toggle or Accessibility service to stop control.

For optional notification cards, enable **Forward notifications to VR while casting**, grant Android notification access, and configure package allow/block lists if desired. Selecting a card reveals the phone and invokes the source notification's Android launch action when one is still available; this requires the current Android APK and receiver protocol on both endpoints. Content remains redacted unless the separate sensitive-content checkbox is explicitly enabled. Cards are sent only while casting and appear for six seconds in the Windows OpenVR receiver. Use **PhoneCast → Settings → Notifications** in the SteamVR dashboard to adjust Size, Distance, Horizontal, and Vertical placement; Apply persists changes and Cancel restores the prior values. This path is automatically tested but still requires physical Android/headset validation. The current transport is unencrypted, so use notification forwarding only on a trusted LAN.

See `android/sender/README.md`, `docs/android-capture.md`, `docs/remote-control.md`, and `docs/notifications.md` for behavior, permissions, policy constraints, and physical-test status.

## CMake options

- `PHONECAST_BUILD_RECEIVER` — build the generated OpenVR receiver and, on Windows, the desktop and OpenVR stream receivers (default `ON`).
- `PHONECAST_BUILD_HELLO_FRAME` — retain the Sprint 0 experiment (default `ON`).
- `BUILD_TESTING` — build/register tests (default controlled by CTest, normally `ON`).

## Tests

Receiver unit tests use fake logging and overlay implementations, so they do not require SteamVR. Android configuration tests also run without a device. Neither suite replaces visual compositor checks or physical-device capture/encoder tests.
