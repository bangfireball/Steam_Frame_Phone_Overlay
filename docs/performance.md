# Sprint 10 performance measurement

Sprint 10 adds repeatable instrumentation; it does not replace physical headset or phone validation. Do not start or restart SteamVR through Windows RDP. Follow the physical-console procedure in [`development.md`](development.md).

## Approved Standard-profile result — 2026-10-04

The project owner approved Sprint 10 after a physical Standard-profile run recorded in `out/logs/vr-receiver.stdout.log`. The log is a local test artifact rather than a committed product file. The receiver ran at 590 × 1280 for approximately 253 connected seconds.

Startup from authenticated connection:

- first video configuration: 340.6 ms;
- first keyframe: 372.2 ms;
- decoder initialization: 4.25 ms;
- first submitted overlay frame: 381.8 ms.

Across 201 dynamic one-second samples (`rx-fps >= 20`):

- receive/decode FPS: 29.93 average;
- submitted render FPS: 28.40 average;
- bitrate: 2.00 Mbps average, 2.41 Mbps 95th percentile;
- decode: 3.78 ms average, 4.05 ms 95th percentile;
- D3D/OpenVR submission: 0.37 ms average;
- queue age: 8.83 ms average, 11.88 ms 95th percentile, 47.8 ms maximum observed;
- process CPU: 1.13% average, 3.67% 95th percentile, 4.27% maximum;
- working set: approximately 69 MiB average and 71.8 MiB maximum;
- steady-state transport drops/resynchronizations: zero;
- OpenVR-reported dropped/mispresented frames: zero.

OpenVR total GPU frame time averaged approximately 0.69 ms while waiting without a phone connection and 1.27 ms during dynamic streaming. Compositor CPU time changed from approximately 0.27 ms to 0.41 ms. This is a same-process waiting comparison, not a controlled receiver-stopped game baseline and not process-attributed GPU utilization.

Occasional submitted-render dips occurred while receive/decode remained near 30 FPS, consistent with the receiver retaining the latest decoded frame when work arrives in a batch. No transport loss accompanied the dips, and the owner approved the run without reporting a visible blocker.

Closure explicitly defers separate Battery Saver and Quality characterization and external-camera glass-to-glass distributions. First submission is not photon-level latency; these deferred items must not be represented as tested.

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
- audio submissions/drops/failures, queued blocks, output latency, and audio bitrate;
- video sync queue depth, cumulative deadline/overflow releases and coalescing,
  and last dequeued picture's hold time and estimated picture-minus-audible PTS
  skew (only when `video_sync_skew_valid=1`; a media-time estimate, not measured
  glass-to-glass delay or verified lip-sync);
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

## Deferred follow-up evidence

Sprint 10 is owner-approved and closed using the Standard-profile evidence above. Future tuning should still collect:

- external-camera glass-to-glass distributions;
- sustained FPS and bandwidth for Battery Saver and Quality;
- a controlled receiver-stopped gameplay baseline;
- broader Wi-Fi/device coverage;
- evidence for adjusting the alternate profile values.
