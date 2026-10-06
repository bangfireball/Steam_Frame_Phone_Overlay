# Steam Frame ARM64 platform

Sprint 11 adds a native Linux ARM64 streaming receiver. It reuses the portable protocol and VR models plus the shared OpenVR backend; no Windows Media Foundation, WinSock, Win32, or D3D types enter this target.

## Native pipeline

```text
Android MediaCodec H.264 -> paired TCP -> stateful V4L2 M2M decoder
    -> NV12/NV12M -> CPU RGBA -> Vulkan staging upload
    -> reusable double-buffered images -> OpenVR SetOverlayTexture
```

The decoder searches `/dev/video0` through `/dev/video63` for a streaming, multi-planar M2M device that advertises H.264 compressed input. `--video-device /dev/videoN` overrides discovery. It subscribes to source-change events, negotiates linear NV12/NV12M capture, uses MMAP queues, and preserves the Android-visible dimensions when the decoder reports coded padding.

The V4L2 path targets the native Qualcomm/iris stateful decoder expected on Steam Frame. Public Valve documentation does not currently specify an application-facing decoder API, so device discovery, accepted formats, and performance remain hardware-test requirements rather than documented guarantees.

If submission returns an error (including the observed Qualcomm `EBUSY` case), the receiver keeps the authenticated phone connection open, drops queued prediction frames, requests a fresh keyframe, and reopens the decoder up to three times with bounded backoff. A watchdog applies the same recovery after three consecutive active receive windows with no decoded output. Recovery attempts and outcomes appear in console diagnostics and the performance CSV. The policy and ARM64 build are validated automatically; actual recovery on Steam Frame hardware remains a required physical test.

Decoded frames retain the CPU color conversion, but repeated `SetOverlayRaw` has been replaced by a Steam Frame Vulkan resource inside `platform/openvr`. It loads `libvulkan.so.1` at runtime, creates the instance/device with OpenVR's required extensions on the compositor GPU, persistently maps one staging buffer, alternates between two reusable device-local RGBA images, and submits `TextureType_Vulkan` with `SetOverlayTexture`. A synchronous upload fence isolates correctness before zero-copy optimization. Stream-size changes clear the compositor texture before recreation, and shutdown keeps Vulkan resources alive until overlays are destroyed and `VR_Shutdown` returns.

The Vulkan path is physically approved as smooth and flicker-free for the measured 590 × 1280 stream and local Hades coexistence run. During the 63-second game window it sustained 30.00 receive/decode FPS and 29.92 submitted FPS with 1.52 ms average Vulkan submission, 1.52% process CPU, stable 102.33 MiB working set, and no transport or OpenVR frame drops. It does not claim dma-buf import or zero-copy decoder sharing; those remain optional later optimizations. Portrait/landscape texture recreation, clean shutdown, and actual standalone-VR-scene coexistence still require physical validation.

## Build

Use the Docker ARM64 cross-build command in `docs/development.md`. The Sprint 11 executable is:

```text
out/build/linux-arm64-sprint11/bin/phonecast-vr-stream-receiver
```

The adjacent `phonecast-receiver.vrmanifest` contains `binary_path_linux_arm` for that executable. OpenVR is linked statically; the receiver otherwise uses the GNU runtime and Linux kernel V4L2/socket APIs.

## Run on Steam Frame

Upload the receiver executable and adjacent manifest/input JSON files with SteamOS Devkit Client using Steam Linux Runtime 3.0 ARM64 (Sniper), or copy them through the established SSH development path. Ensure the executable bit is present.

```bash
chmod +x phonecast-vr-stream-receiver
./phonecast-vr-stream-receiver --pair-code 123456
```

Optional diagnostics:

```bash
./phonecast-vr-stream-receiver \
  --pair-code 123456 \
  --video-device /dev/videoN \
  --performance-log "$HOME/phonecast-performance.csv"
```

Settings default to `$XDG_CONFIG_HOME/phonecast-vr/overlay-settings.ini`, or `$HOME/.config/phonecast-vr/overlay-settings.ini` when `XDG_CONFIG_HOME` is unset. `SIGINT` and `SIGTERM` request clean shutdown.

The phone must connect to the Steam Frame LAN address and matching TCP port (default `49321`). The transport remains unencrypted and must be tested only on a trusted LAN.

## Standalone launcher baseline

The Linux build copies `steam-frame-installer` beside the receiver. Its installer
adds an absolute-path SteamOS desktop launcher and 48/128/256 pixel icons. A
per-user native lock prevents duplicate receivers; later launch attempts signal
the resident process to open/focus the PhoneCast dashboard. Native first launch
creates an owner-only persistent pairing code, so ordinary restarts do not
require a command-line credential.

This is implemented and cross-built but not yet physically approved. Keep
manual Android address entry available and do not enable autostart before the
manual dashboard `+` path passes on-headset testing. See
`docs/standalone-ux.md`.

## Required physical review

Do not mark Sprint 11 complete from the cross-build alone. Record:

1. selected `/dev/videoN`, driver, H.264 input format, and NV12/NV12M capture format;
2. first-frame timing, sustained FPS, CPU/memory, bitrate, queue age, and OpenVR timing;
3. phone visibility and orientation changes;
4. dashboard, settings, placement, remote touch, Back, notification cards/actions, and reconnect;
5. behavior across dashboard transitions, headset sleep/wake, and app switching;
6. overlay coexistence with a native standalone VR scene in both launch orders;
7. absence of prior raw-upload flicker and `VROverlayError_RequestFailed`, stable working-set memory, Vulkan upload cost, and clean texture teardown.
