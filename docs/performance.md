# Sprint 10 performance measurement

Sprint 10 adds repeatable instrumentation; it does not replace physical headset or phone validation. Do not start or restart SteamVR through Windows RDP. Follow the physical-console procedure in [`development.md`](development.md).

## Instrumented receiver run

Start the VR receiver at the physical console with a CSV destination:

```powershell
.\out\build\windows-x64\bin\phonecast-vr-stream-receiver.exe `
  --pair-code 123456 `
  --performance-log out\performance\standard.csv
```

The receiver still prints its rolling diagnostics. The CSV records one sample per second and flushes every row so a terminated VR session retains evidence.

Recorded receiver measurements include:

- connection-to-first codec configuration, keyframe, and submitted overlay frame;
- decoder initialization time;
- received, decoded, and rendered FPS;
- video bitrate, receiver queue age/depth, dropped frames, and resyncs;
- average decode and D3D/OpenVR submission call time;
- PhoneCast process CPU, working set, and private memory;
- OpenVR's latest scene/compositor frame timing, including total/compositor GPU time, compositor CPU time, dropped/mispresented frames, and reprojection flags.

`first_submitted_ms` is the time at which PhoneCast successfully submits the first decoded texture. OpenVR does not acknowledge photon emission, so this value must not be described as first-visible or glass-to-glass latency.

The OpenVR GPU fields are frame times, not process GPU-utilization percentages. They characterize the active scene/compositor and are useful for an A/B comparison with PhoneCast stopped, but do not attribute all GPU work to PhoneCast.

## Streaming profiles

The Android settings screen provides three sender profiles:

| Profile | Long-edge cap | FPS | Bitrate bounds |
| --- | ---: | ---: | ---: |
| Battery Saver | 960 | 20 | 1–4 Mbps |
| Standard | 1280 | 30 | 2–10 Mbps |
| Quality | 1920 | 30 | 4–16 Mbps |

Standard preserves the physically accepted 1280/30 path. Battery Saver and Quality are measurement candidates, not physically approved defaults. Profile changes apply to the next capture session and persist on Android. Rotation recreates the encoder using the active profile.

## Baseline protocol

For each profile:

1. Start SteamVR and the same representative VR game at the physical console.
2. Record a five-minute **PhoneCast stopped** baseline using SteamVR's frame-timing tools.
3. Start the receiver with a fresh CSV path and begin Android casting.
4. Record one minute each of a static phone screen, continuous vertical scrolling, video playback, and dashboard-open interaction.
5. Record rotation and one receiver reconnect.
6. Note the phone model, Android build, Wi-Fi band/link conditions, PC CPU/GPU, headset/runtime versions, VR game/settings, profile, and test timestamps.
7. Compare VR GPU/CPU frame time, dropped/mispresented frames, and reprojection against the stopped baseline. Compare PhoneCast CPU/memory, bitrate, queue age, FPS, drops, and startup fields between profiles.

Notification traffic should be tested once during video playback to verify that its separate queue does not change video drops or resyncs.

## Glass-to-glass latency

Android encoder presentation timestamps and Windows clocks have no established offset. Do not subtract them.

Use an external 120/240 FPS camera that sees both the phone and headset lens (or a photodiode/LED fixture). Trigger a high-contrast full-screen change, count camera frames from phone change to headset change, and report the median, 95th percentile, sample count, and camera frame period. Test isolated/static changes separately from continuous motion because decoder look-ahead previously affected sparse updates.

## Acceptance evidence still required

Automated builds can validate instrumentation and profile selection, but Sprint 10 remains in progress until physical measurements establish:

- startup and glass-to-glass distributions;
- sustained FPS and bandwidth for each profile;
- CPU/memory and VR-frame-time impact against a no-PhoneCast baseline;
- acceptable behavior under representative Wi-Fi load;
- whether Battery Saver and Quality values should be retained or adjusted.
