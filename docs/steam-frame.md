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
| Linux ARM64 build | `[x]` | Debian cross-build produced an AArch64 ELF; emulated CLI smoke test passes |
| Native Steam Frame overlay without a game | `[!]` | Blocked: headset was not reachable on the network |
| Native overlay over a standalone game | `[!]` | Blocked: headset was not reachable on the network |
| Overlay controller input during a game | `[!]` | Blocked until display/coexistence succeeds |

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

SteamVR 2.17.10 and its VRLink driver are installed on the Windows host, but no wireless Steam Frame HMD was connected during the test window. SteamVR reported the wireless-HMD-not-connected state, and the headset hostname was not resolvable. Visual overlay, game coexistence, and performance tests could therefore not be performed.

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

These are hypotheses to test, not confirmed capabilities:

1. A regular OpenVR overlay should remain visible over a PC SteamVR game because that is the API's documented model.
2. A static `SetOverlayRaw` image should have negligible steady-state CPU/GPU cost after upload.
3. The same Hello Frame source should compile on Windows x64 and native Linux ARM64.
4. HMD-relative placement at Z = -1 metre should put the panel ahead of the user in OpenVR coordinates.
5. Native Steam Frame SteamVR may expose `IVROverlay` similarly to PC SteamVR.

The fifth assumption is the central Sprint 0 risk and must not be promoted to documented or tested behavior without a native test.

---

## Unknown behavior

1. Whether native Steam Frame applications can initialize as `VRApplication_Overlay`.
2. Whether `IVROverlay::CreateOverlay` succeeds in the standalone runtime.
3. Whether the compositor displays that overlay over another standalone application.
4. Whether SteamOS leaves the overlay process running when a game launches.
5. Whether launch order changes the result.
6. Whether controller events can reach the overlay while a game owns focus.
7. Whether an application manifest or special launch metadata is required.
8. Whether the overlay survives dashboard transitions, game switching, headset sleep, and wake.
9. Whether Steam Frame policy restricts auto-start or long-running background overlay processes.
10. Whether OpenXR overlay-session support is advertised by the installed runtime.
11. Whether a future video texture can be transferred to the compositor without avoidable copies.
12. Which native hardware-decoding API and texture-sharing route is preferable on Steam Frame.

Items 11–12 affect later sprints and are not implemented in Hello Frame.

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
- a low-frequency event loop with clean shutdown.

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

## Current conclusion

PC OpenVR overlay behavior is sufficiently documented to justify the Hello Frame baseline. Native Linux ARM64 development is documented and the OpenVR client library has ARM64 source-build support.

However, no primary source reviewed here guarantees persistent third-party overlays over another **standalone** Steam Frame application. That behavior remains **unknown** until the native test is completed.
