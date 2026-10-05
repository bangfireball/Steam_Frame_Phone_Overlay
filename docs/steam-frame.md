# Steam Frame overlay feasibility

Last research review: 2026-10-01

Sprint: 0 — Hello Frame

## Status

| Item | Status | Result |
|---|---:|---|
| Valve/OpenVR/OpenXR documentation review | `[x]` | Initial review complete |
| Minimal regular-overlay implementation | `[x]` | Source created; build/runtime validation pending |
| Windows x64 build | `[x]` | MinGW-w64 release build and CLI tests pass |
| PC SteamVR overlay without a game | `[!]` | Blocked: Steam Frame headset was not connected |
| PC SteamVR overlay over a running game | `[!]` | Blocked: Steam Frame headset was not connected |
| Linux ARM64 build | `[x]` | Debian cross-build produced an AArch64 ELF; emulated and native CLI tests pass |
| Native Steam Frame overlay without a game | `[x]` | OpenVR initialized, raw image loaded, overlay stayed visible, clean shutdown |
| Native overlay over a standalone flat game | `[x]` | Registered overlay stayed alive for the complete timed test while Balatro ran locally |
| Native overlay over a standalone VR game | `[~]` | Native mechanism is promising; a VR scene application test is still pending |
| Overlay controller input during a game | `[ ]` | Not implemented or tested in Sprint 0 display prototype |

The highest-priority question remains unresolved until tested on Steam Frame hardware:

> Can a native Linux ARM64 process create an overlay that remains visible over another standalone Steam Frame VR application?

## Terminology and evidence rules

- **Documented** — explicitly stated by Valve, Khronos, or the relevant SDK.
- **Tested** — directly observed with this repository's Hello Frame build; includes environment and reproduction details.
- **Assumed** — a working hypothesis, not evidence.
- **Unknown** — neither documented nor tested adequately.

A PC game streamed to Steam Frame is still a PC SteamVR test. It is not a native standalone test.

---

## Documented behavior

### Steam Frame development targets

Valve recommends targeting either Android 10 or Linux ARM64 for custom-engine standalone development. Valve recommends OpenXR, while also stating that OpenVR can be used with the SDK header and the appropriate `libopenvr_api.so`.

Valve documents Developer Mode as enabling SSH, ADB, and remote-desktop access. For Linux ARM64 deployment, Valve recommends SteamOS Devkit Client.

SteamOS Devkit Client's documented Linux deployment runtime is **Steam Linux Runtime 3.0 ARM64 (Sniper)**. The client uploads a local folder and launches a configured relative binary path.

Sources:

- [Valve — Custom Engines](https://partner.steamgames.com/doc/steamhardware/steamframe/engines/custom?language=english)
- [Valve — Setting up Steam Frame for development](https://partner.steamgames.com/doc/steamhardware/steamframe/setup?language=english)
- [Valve — Loading and running games on Steam Frame](https://partner.steamgames.com/doc/steamhardware/steamframe/loadgames?language=english)
- [Valve — Steam Frame debugging](https://partner.steamgames.com/doc/steamhardware/steamframe/debugging?language=english)

### OpenVR overlays

Valve's OpenVR documentation describes `IVROverlay` as drawing 2D images over the 3D scene “no matter which application is running.” It distinguishes regular overlays from dashboard overlays and provides APIs for creation, transforms, visibility, textures, and events.

Relevant APIs:

- `VR_Init(..., VRApplication_Overlay)` initializes an overlay application.
- `CreateOverlay` creates a regular overlay with a globally unique key.
- `SetOverlayRaw` supplies bounded raw pixel data without requiring a graphics API.
- `SetOverlayTexture` supplies a graphics texture and is a likely later video path, but is unnecessary for a static experiment.
- `SetOverlayTransformTrackedDeviceRelative` attaches a transform relative to a tracked device such as the HMD.
- `ShowOverlay`, `HideOverlay`, and `IsOverlayVisible` manage and inspect visibility.
- `PollNextOverlayEvent` provides overlay events.
- `SetOverlayInputMethod`, `SetOverlayMouseScale`, and intersection helpers support overlay input.

Valve's `helloworldoverlay` sample uses `VRApplication_Overlay`, creates a dashboard overlay, renders through OpenGL, and polls overlay events. PhoneCast's Sprint 0 experiment instead uses `CreateOverlay`, because the test requires an always-visible scene overlay rather than a dashboard tab.

Sources:

- [Valve — IVROverlay overview](https://github.com/ValveSoftware/openvr/wiki/IVROverlay_Overview)
- [Valve — OpenVR API documentation](https://github.com/ValveSoftware/openvr/wiki/API-Documentation)
- [Valve — OpenVR `helloworldoverlay` sample](https://github.com/ValveSoftware/openvr/tree/master/samples/helloworldoverlay)
- [Valve — OpenVR SDK](https://github.com/ValveSoftware/openvr)

### OpenVR Linux ARM64 build support

OpenVR SDK 2.15.6 source contains explicit Linux/aarch64 detection (`LINUXARM64`) and builds `openvr_api` from source with CMake. This repository pins commit `0924064316de3effbcd1acf1e309182a2deb1c05` and builds the client library statically for reproducibility.

This proves that the client API library is intended to compile for the architecture. It does **not** prove that a particular SteamVR runtime permits cross-application overlays.

Sources:

- [OpenVR SDK 2.15.6 source](https://github.com/ValveSoftware/openvr/tree/0924064316de3effbcd1acf1e309182a2deb1c05)
- [OpenVR issue #1925 — ARM64 library packaging/build discussion](https://github.com/ValveSoftware/openvr/issues/1925)
- [OpenVR issue #1922 — Linux ARM64 page-size report](https://github.com/ValveSoftware/openvr/issues/1922)

Issue reports are supporting evidence, not equivalent to official runtime guarantees. Building from source also avoids relying on an ambiguously packaged prebuilt ARM64 library, but runtime behavior still requires hardware testing.

### OpenXR overlays

OpenXR composition layers normally belong to an application's session. Khronos defines `XR_EXTX_overlay` for overlay sessions, but the extension is marked **not ratified** in the OpenXR registry. Its existence does not imply that SteamVR or Steam Frame supports it.

Sprint 0 therefore starts with OpenVR because `IVROverlay` directly models the required cross-application concept. This does not establish OpenVR as the final backend.

Source:

- [Khronos — `XR_EXTX_overlay`](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_EXTX_overlay.html)

### Controller input

OpenVR documents mouse-style overlay input and event polling. Valve's sample configures `VROverlayInputMethod_Mouse`, sets a mouse scale, and handles mouse movement and button events.

This documents API availability. It does not establish that a regular native overlay receives controller input while a standalone Steam Frame game has focus.

---

## Tested behavior

### Build and CLI validation — 2026-10-01

**Result:** PASS (build only; no visual VR claim)

- Windows x64 release build completed with CMake 4.4.3, Ninja 1.13.2, and MinGW-w64 GCC 16.2.0.
- The Windows bundle contains `hello-frame.exe` and Valve's `openvr_api.dll` from SDK 2.15.6.
- The executable links the MinGW C++ support statically, avoiding additional compiler-runtime DLLs.
- Two CTest CLI tests pass: `--help` succeeds and an invalid argument is rejected.
- With no active HMD runtime, startup failed cleanly with OpenVR error 126, `Hmd Not Found Presence Failed`, and exit code 1.
- A Debian 12 Docker cross-build with GCC 12.2.0 produced an AArch64 PIE executable for GNU/Linux.
- `file` identified the result as `ELF 64-bit LSB pie executable, ARM aarch64`.
- Its dynamic dependencies are limited to the expected GNU/Linux runtime libraries: `libstdc++`, `libm`, `libgcc_s`, and `libc`; OpenVR is linked statically on Linux.
- Running `--help` under `qemu-aarch64` with an ARM64 sysroot succeeded.

This confirms source/build portability to ARM64. It does **not** confirm that Steam Frame's runtime accepts or displays the overlay.

### PC runtime availability — 2026-10-01

**Result:** BLOCKED

SteamVR 2.17.10 and its VRLink driver are installed on the Windows host, but no wireless Steam Frame HMD was connected to PC SteamVR during the test window. SteamVR reported the wireless-HMD-not-connected state. The PC visual overlay and PC game-coexistence tests were therefore not performed.

### Native Steam Frame overlay — 2026-10-02

**Result:** PASS for native overlay creation; PARTIAL for the final cross-VR-application requirement

Environment:

- SteamOS build `20260922.6101926`
- Linux kernel `6.18.0-gfbdbca41fd45`, AArch64
- Native SteamVR runtime `2.17.10`, architecture `linuxarm64`
- OpenVR client SDK `2.15.6`
- Deployment account/host: `steamos@frame`
- Binary: cross-built AArch64 PIE with OpenVR linked statically

Observed:

1. The uploaded binary executed natively on Steam Frame.
2. `VR_Init(..., VRApplication_Overlay)` succeeded against `/opt/steamvr`.
3. `CreateOverlay`, the HMD-relative transform, `SetOverlayRaw`, and `ShowOverlay` all succeeded.
4. SteamVR emitted `VREvent_ImageLoaded` for the generated 640×240 image.
5. A 15-second timed test completed without runtime errors and shut down cleanly.
6. The runtime registered the process as `VRApplication_Overlay` and loaded legacy Frame controller bindings, although input was not enabled by the prototype.
7. During a longer run the process used approximately 16.7 MiB resident memory and about 0.9% CPU as sampled over its first minute. This is an initial observation, not a final performance benchmark.
8. The user visually confirmed that the “HELLO FRAME” panel remained visible while playing a standalone, on-device 2D game.
9. The user confirmed that the panel followed their head. This is the expected result of `SetOverlayTransformTrackedDeviceRelative` using the HMD as the tracked device.
10. The user could not interact with the panel. This is expected: Sprint 0 does not configure an overlay input method or implement event-driven controls.

Application lifecycle finding:

- An initially unregistered overlay was sent a quit request when a locally running application transition occurred.
- Steam Frame requires the ARM-specific manifest field `binary_path_linux_arm`; `binary_path_linux` alone is insufficient on this runtime.
- With `binary_path_linux_arm` present, SteamVR installed the stable app key `com.phonecastvr.hello-frame`.
- The manifest marks the process as a dashboard-overlay application even though the program creates a regular scene overlay. This allows SteamVR to classify it as an overlay application intended to coexist with another application.
- After registration, Hello Frame ran for the full timed test while the standalone, on-device Balatro process was active.

This is strong evidence that native third-party OpenVR overlays function on Steam Frame and can coexist with another locally running application. It is **not yet the final success condition**, because Balatro is a flat application rather than a standalone VR scene application. A registered-overlay launch-order test with a native/on-device VR title remains required.

### Test record template

```text
Date:
Result: PASS / PARTIAL / FAIL / BLOCKED
Host mode: PC SteamVR / Steam Frame native ARM64
OS and build:
SteamVR version:
OpenVR SDK version:
Headset:
Game:
Hello Frame commit:
Launch order:
Duration:

Observed:
CPU:
GPU:
Memory:
Errors/logs:
Limitations:
Reproduction steps:
```

---

## Assumed behavior

These hypotheses remain unconfirmed:

1. A regular OpenVR overlay should remain visible over a PC SteamVR game because that is the API's documented model.
2. The static `SetOverlayRaw` image has negligible GPU impact after upload; CPU and memory were sampled, but GPU impact was not isolated.

Linux ARM64 compilation, native `IVROverlay` access, HMD-relative placement, and coexistence with an on-device flat game have moved from assumptions to tested behavior.

---

## Unknown behavior

Native `VRApplication_Overlay` initialization, regular overlay creation, raw-image loading, and coexistence with a locally running flat application are now tested successfully.

The remaining unknowns are:

1. Whether the compositor keeps the registered overlay visible over a standalone VR scene application.
2. Whether a registered overlay launched before a VR game survives the game transition.
3. Whether controller events can reach the overlay while a VR game owns focus.
4. Whether the overlay survives dashboard transitions, VR game switching, headset sleep, and wake.
5. Whether Steam Frame policy permits auto-start and indefinite background overlay operation.
6. Whether OpenXR overlay-session support is advertised by the installed runtime.
7. Whether a future video texture can be transferred to the compositor without avoidable copies.
8. Which native hardware-decoding API and texture-sharing route is preferable on Steam Frame.

Items 7–8 affect later sprints and are not implemented in Hello Frame.

---

## Hello Frame implementation decision

The Sprint 0 executable intentionally uses:

- C++17 and CMake;
- OpenVR SDK 2.15.6 pinned by commit;
- `VRApplication_Overlay`;
- a regular `CreateOverlay` overlay;
- a generated 640×240, four-byte-per-pixel image;
- `SetOverlayRaw` for a one-time static upload;
- a 0.55-metre physical width;
- an HMD-relative transform one metre forward;
- a low-frequency event loop with clean shutdown;
- persistent registration of the adjacent `.vrmanifest` through `IVRApplications`;
- `is_dashboard_overlay` classification and the Steam Frame-specific `binary_path_linux_arm` manifest path.

It intentionally avoids DirectX, OpenGL, Vulkan, Qt, SDL, networking, decoding, and reusable production interfaces. `SetOverlayRaw` is a feasibility mechanism, not the planned streaming renderer.

---

## Required test sequence

### PC baseline

1. SteamVR running, no game: launch and stop Hello Frame.
2. Launch Hello Frame, then launch a VR game.
3. Launch a VR game, then launch Hello Frame.
4. Open and close the dashboard.
5. Exit and restart the game while leaving Hello Frame running.
6. Stop SteamVR and observe shutdown behavior.
7. Record CPU, GPU where practical, memory, errors, and lifecycle behavior.

### Steam Frame native ARM64

1. Enable Developer Mode and pair SteamOS Devkit Client.
2. Build or cross-build the Linux ARM64 binary.
3. Verify architecture and dynamic dependencies.
4. Upload using Steam Linux Runtime 3.0 ARM64 (Sniper).
5. Run Hello Frame without a standalone game.
6. Launch Hello Frame before a standalone game.
7. Launch Hello Frame after a standalone game.
8. Test game exit, game switching, dashboard, and headset sleep/wake.
9. Test controller-event delivery if overlay display succeeds.
10. Save exact versions, commands, logs, and launch metadata.

---

## Result classification

### Confirmed

Use only after observing a native ARM64 overlay remain visible over a standalone game:

```text
STEAM FRAME STANDALONE OVERLAY = CONFIRMED
```

### Partial

Use when only part of the requirement works—for example, the overlay appears in the system UI but disappears in games, requires a specific launch order, or cannot receive input.

### Failed

Use when a reproducible native test demonstrates that the required behavior is unavailable. Continue the PC-hosted implementation and preserve platform boundaries.

### Blocked

Use when a required environment, particularly Steam Frame hardware/runtime access, is unavailable. Lack of access is not evidence of failure.

---

## Sprint 11 native receiver implementation

The review branch now builds the complete `phonecast-vr-stream-receiver` for Linux ARM64. The transport was moved from the Windows platform directory to a shared WinSock/POSIX implementation without moving socket details into Core. Linux selects a stateful V4L2 H.264 M2M decoder, handles source-change capture setup, negotiates linear NV12/NV12M, converts visible pixels to RGBA, and feeds the existing OpenVR renderer. The same application retains dashboard controls, placement, remote input, notification cards/actions, settings, and CSV/OpenVR diagnostics.

A clean Debian 12 AArch64 cross-build produced an `ELF 64-bit ... ARM aarch64` streaming receiver. This is build evidence only. It does not establish that Steam Frame exposes a compatible `/dev/videoN`, that the Qualcomm/iris driver accepts this queue sequence, that `SetOverlayRaw` is acceptable at video cadence, or that the overlay coexists with a native VR scene.

Public Valve documentation identifies Linux ARM64 as a supported target but does not document an application-facing hardware decoder API. The implementation therefore treats stateful V4L2 as a tested-on-hardware hypothesis, exposes `--video-device`, and reports device/negotiation failures rather than silently falling back to CPU decode. Record the selected node, driver, formats, timing, and errors during deployment. GPU/dma-buf compositor sharing remains unknown and is not claimed complete.

### Sprint 11 installation and native startup — 2026-10-04

The ARM64 streaming bundle was installed over SSH at `/home/steamos/phonecast`; the previous Sprint 1 receiver directory was retained as a timestamped backup. No credential or `.env` content was copied into the repository or installation.

Observed system details:

- hostname `frame`, AArch64 Linux, SteamOS;
- kernel `6.18.0-gfbdbca41fd45`;
- native SteamVR under `/opt/steamvr`;
- Qualcomm `iris_driver` stateful decoder, card `iris_decoder`;
- decoder alias `/dev/video-dec0` targeting `/dev/video22`;
- decoder capabilities include multi-planar video memory-to-memory and streaming;
- compressed output/input-queue formats advertised by the decoder are H.264, HEVC, and VP9;
- capture formats advertised are Qualcomm compressed 8-bit, NV12, NV21, RGBA, and another Qualcomm format;
- the current default capture format reported by `v4l2-ctl` is single-plane NV12.

An eight-second native receiver smoke test passed the non-video startup gate: OpenVR registered and identified `com.phonecastvr.receiver`, created the regular overlay and PhoneCast dashboard tab/icon, listened on TCP port 49321, emitted process/OpenVR diagnostics, and shut down through `SIGTERM`. Working set was approximately 17.5 MiB, sampled process CPU was 0–0.25%, and OpenVR reported zero dropped or mispresented frames during the short idle run. The installed receiver was then left running with `/dev/video-dec0` selected.

A subsequent physical Android test validated authenticated transport, the Qualcomm/iris H.264 queue, NV12 conversion, and visible phone video. During ordinary dynamic content the native path commonly sustained approximately 30 received/decoded/rendered FPS, roughly 1.7–2.4 Mbps, 4–5 ms decode, 1.5–1.8 ms raw-overlay submission, about 2–2.5% sampled process CPU, zero transport drops/resyncs, and zero OpenVR-reported dropped frames. The user reported the phone display and functional controls working. A width of **0.20 m** was physically preferred, so new native installations use that width while preserving any saved setting.

The test also exposed a blocking renderer defect: repeated Linux `SetOverlayRaw` submissions visibly flickered, working-set growth was observed into the hundreds of MiB, and the compositor eventually returned `VROverlayError_RequestFailed`, terminating the receiver.

A replacement Vulkan renderer is now implemented and ARM64 cross-built. It loads `libvulkan.so.1` at runtime, uses OpenVR's required extensions and compositor-selected physical device, uploads CPU RGBA through a persistently mapped staging buffer into two reusable device-local images, and submits them with `SetOverlayTexture`/`TextureType_Vulkan`. Texture-size changes clear the compositor texture before recreation; shutdown releases Vulkan resources only after overlay destruction and `VR_Shutdown`. This removes repeated raw submission from the video path without putting Vulkan handles into Core or changing the validated V4L2 decoder.

The replacement was installed at `/home/steamos/phonecast` on 2026-10-04, with the prior raw-upload build retained at `/home/steamos/phonecast.backup-20261004-222435`. Native startup selected `Turnip Adreno (TM) 750`, enabled five required instance and eight device extensions, and created a reusable 590 × 1328 double-buffered texture set.

On 2026-10-04, the project owner physically approved the renderer as perfectly smooth and reported that scrolling and other remote gestures worked. The captured session contained 243 connected seconds and a 63-second locally running Hades window. During that game window, the 590 × 1280 phone stream averaged 30.00 receive/decode FPS, 29.92 submitted FPS, 2.10 Mbps, 4.26 ms decode, 1.52 ms Vulkan submission, 1.06 ms queue age, 1.52% PhoneCast process CPU, and 102.33 MiB working set. Queue depth, transport drops/resyncs, and OpenVR-reported dropped/mispresented frames were zero. Across all dynamic connected samples, receive/decode averaged 29.11 FPS and render averaged 28.36 FPS. Startup reached configuration at 291.45 ms, the first keyframe at 322.09 ms, decoder startup took 7.72 ms, and the first frame was submitted at 466.77 ms.

The process remained at approximately 102.33 MiB RSS after streaming rather than continuing the prior raw-upload growth into hundreds of MiB. Only one texture set was created, and no Vulkan upload, `SetOverlayTexture`, or `VROverlayError_RequestFailed` error occurred. The single transport drop/resync was confined to the initial partial startup second; steady dynamic streaming and the Hades window had none.

Hades is a locally running flat game, not the pending standalone VR scene. Steam marked it for VR presentation and briefly created an OpenXR test instance, but that instance disconnected after about two seconds. This provides strong local-game coexistence evidence without satisfying the stricter VR-scene acceptance criterion.

The prior raw-upload renderer blocker is resolved for this measured run. Sprint 11 remains incomplete pending portrait/landscape texture recreation, clean shutdown, detailed notification/reconnect/sleep-wake lifecycle cases, and coexistence with an actual standalone VR scene in both launch orders. The headset's DHCP address is intentionally not recorded here because it can change; obtain the current `wlan0` address when configuring Android.

## Current conclusion

Native Linux ARM64 OpenVR overlays are no longer merely theoretical: Hello Frame successfully initialized, created a regular overlay, loaded its generated image, registered a stable application manifest, and remained visibly head-locked while the user played a locally running 2D game on Steam Frame.

The project owner accepts this as a successful validation of the foundational native-overlay mechanism. Under the design document's stricter original criterion, coexistence with a standalone **VR scene application** remains the final untested case. The result is therefore recorded as **foundational success, with one VR-specific acceptance test pending**.

---

## Sprint 1 portability follow-up — 2026-10-02

The production receiver foundation cross-built successfully with Debian 12's AArch64 GNU 12.2 toolchain. `file` identified `phonecast-receiver` as an ARM64/AArch64 GNU/Linux PIE executable. The build uses the shared production OpenVR backend and includes a receiver manifest with `binary_path_linux_arm`.

The Sprint 1 receiver was subsequently deployed and launched natively on Steam Frame. The user visually confirmed the panel and moving test pattern. The generated scrolling bars are intentional proof of continuous frame changes; visible panel flicker is not intentional and was observed during the test. The likely cause is repeated `SetOverlayRaw` replacement, which is suitable for feasibility testing but not a final streaming texture path. Sprint 1 is accepted with this limitation recorded for the video-rendering integration/performance work.
