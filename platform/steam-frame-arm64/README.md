# Steam Frame ARM64 platform

Sprint 11 adds a native Linux ARM64 streaming receiver. It reuses the portable protocol and VR models plus the shared OpenVR backend; no Windows Media Foundation, WinSock, Win32, or D3D types enter this target.

## Native pipeline

```text
Android MediaCodec H.264 -> paired TCP -> stateful V4L2 M2M decoder
    -> NV12/NV12M -> CPU RGBA -> OpenVR SetOverlayRaw
```

The decoder searches `/dev/video0` through `/dev/video63` for a streaming, multi-planar M2M device that advertises H.264 compressed input. `--video-device /dev/videoN` overrides discovery. It subscribes to source-change events, negotiates linear NV12/NV12M capture, uses MMAP queues, and preserves the Android-visible dimensions when the decoder reports coded padding.

The V4L2 path targets the native Qualcomm/iris stateful decoder expected on Steam Frame. Public Valve documentation does not currently specify an application-facing decoder API, so device discovery, accepted formats, and performance remain hardware-test requirements rather than documented guarantees.

Decoded frames currently use a CPU color conversion and OpenVR's raw upload path. This is the smallest dependency-free native implementation and lets the complete product behavior be validated first. It is not represented as the final zero-copy path: dma-buf import or another compositor-compatible GPU texture path should be implemented only after on-device API/format/timing evidence establishes the correct route.

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

## Required physical review

Do not mark Sprint 11 complete from the cross-build alone. Record:

1. selected `/dev/videoN`, driver, H.264 input format, and NV12/NV12M capture format;
2. first-frame timing, sustained FPS, CPU/memory, bitrate, queue age, and OpenVR timing;
3. phone visibility and orientation changes;
4. dashboard, settings, placement, remote touch, Back, notification cards/actions, and reconnect;
5. behavior across dashboard transitions, headset sleep/wake, and app switching;
6. overlay coexistence with a native standalone VR scene in both launch orders;
7. any visible flicker or compositor cost from repeated `SetOverlayRaw` uploads.
