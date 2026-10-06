# Sprint 13 — Optional phone playback audio

**Status: implemented and automatically tested; physical audio acceptance pending.**

The project owner chose Sprint 13 next. Sprint 12 decoder fault validation and
Vulkan device-loss recovery/investigation remain deferred, not fixed by audio.
No receiver, SteamVR, audio service, or game was restarted on physical hardware
while implementing this sprint. The owner subsequently reported audible, decent
phone audio but a severe audio-enabled video slowdown. The cadence correction
below is software-tested; physical audio/video acceptance remains open.

## Use

1. Install the new Android APK and receiver. Old receivers remain video-only.
2. Android → Settings → **Phone playback audio**: enable **Stream phone playback
   audio**, then grant Android's audio-recording permission. This is separately
   opt-in and off by default. Enabling applies on the next casting session;
   disabling stops current audio capture and discards queued sound immediately.
3. Start casting and approve the normal screen-sharing consent. Only a capable
   paired receiver activates playback recording. The Android status card reports
   capture, video-only fallback, silence/policy limitations, or recorder failure.
4. Receiver dashboard → Settings → **Phone Audio**: Mute, Volume, and Output
   controls apply live; Apply persists and Cancel restores the previous controls.
   Audio plays independently of phone overlay visibility and placement.

Built artifacts:

- Android: `android/sender/app/build/outputs/apk/debug/app-debug.apk`
- Windows: `out/build/windows-x64/bin/phonecast-vr-stream-receiver.exe`
- Frame: `out/packages/phonecast-steam-frame-arm64-sprint13.tar.gz`

Quit PhoneCast before updating the Frame bundle, then extract it and run
`sh phonecast-steam-frame-arm64-sprint13/steam-frame-installer/install-phonecast.sh`
as the ordinary headset user. Settings/pairing are retained. The local download
page now serves the rebuilt Android APK and Sprint 13 archive through both the
versioned bundle link and stable curl-bootstrap route. The bootstrap extracts the
Sprint 13 directory. Publishing these builds does not install/restart the physical
receiver or approve audio behavior.

Default output on Windows uses OpenVR's HMD playback-device hint when available,
otherwise the system multimedia default. **System Default** explicitly bypasses
that hint. A stale/unresolvable hint reports an output failure rather than silently
redirecting sound; select System Default to recover. Linux automatic/default
routing uses the PulseAudio/PipeWire default sink. The selected output is not
assumed to be a headset on every machine.

For an explicit WASAPI endpoint ID or Pulse sink name, launch with:

```text
phonecast-vr-stream-receiver --audio-device ID
```

`--audio-device default` forces the system default. An explicit command-line
selection overrides the in-headset route preference for that process. The console
reports the output backend/route, including the actual Pulse sink name. The
headset Status row shows capture/off/mute/output state; there is no in-headset
arbitrary endpoint-list picker in this baseline. The desktop-preview receiver
intentionally advertises no audio support.

## Android restrictions and privacy

- Playback capture uses API 29+ `AudioPlaybackCaptureConfiguration` and the existing
  approved `MediaProjection`. It requires `RECORD_AUDIO` and the same Android user
  profile as the source. The service remains `mediaProjection`, not microphone.
- PhoneCast matches only `USAGE_MEDIA`, `USAGE_GAME`, and `USAGE_UNKNOWN`. The source
  must permit capture by ordinary apps; its most restrictive manifest, AudioManager,
  and player policy applies. Calls, microphones, protected audio, and capture-blocked
  playback are not captured. There is no root/ADB/Accessibility/microphone fallback.
- Silence does not reveal why capture is silent. After two seconds of zero PCM,
  status reads **No capturable playback; source may be silent or disallow capture**.
  Nonzero samples restore active status. This is not a DRM/permission detector.
- Audio is copied, not redirected: the phone may still play through its own output.
  PhoneCast does not change phone volume, request audio focus, or promise local muting.
- Permission/recorder failures disable audio without deliberately stopping healthy
  video. Android itself may kill an application on permission revocation; PhoneCast
  cannot promise process survival when the OS does so.
- Projection stop/phone lock, opt-out, service teardown, disconnect, or an unsupported
  peer stops/releases the audio recorder. A reconnect creates a fresh audio epoch;
  it never replays cached sound. Rotation changes only the video encoder/surface.
- The existing paired transport is **unencrypted, trusted-LAN only**. Audio samples
  are neither logged nor persisted; pairing is not cryptographic authentication.

## Format and transport

The first baseline is PCM signed 16-bit **little-endian**, 48 kHz stereo, in 10 ms
blocks (480 stereo sample frames / 1,920 sample bytes). Exact recorder initialization
is checked; unsupported formats fail clearly rather than being misinterpreted.
PCM avoids codec priming and new codec distributions while validating capture and
playout. Opus remains a later option if bandwidth/load measurements justify it;
AAC-LC was not chosen because of its additional codec buffering/integration work.

Sample bandwidth is 1.536 Mbps. With 12-byte audio metadata and the existing
32-byte common header at 100 packets/s, block traffic is approximately **1.571 Mbps**,
excluding config/status and TCP/IP overhead. This is additional to video and must
be measured under actual Wi-Fi/VRLink and standalone game load.

A capability request/reply in existing PING/PONG messages gates all new audio types
behind pairing. Empty/unsupported replies mean video-only. See `protocol.md` for
the exact Java/C++ layouts and compatibility behavior. Separate sequence/epoch state
and queues ensure audio drops never invalidate the H.264 prediction chain.

- Sender: at most ten audio blocks, dropping oldest audio on congestion; config and
  latest status are independent. A shared wake semaphore avoids the previous 100 ms
  static-video polling delay. Each writer pass services audio, notification, and video
  rather than allowing one media queue to starve another.
- Receiver transport: at most ten blocks plus config/latest status; blocks older than
  100 ms are discarded before application dispatch. Config/stop clears stale audio.
- Output worker: at most ten blocks, independently age-limited to 100 ms. Mute clears
  its queue and closes/discards backend buffers. Device changes reset output.
  Forward timestamp discontinuities above 100 ms flush queued application/backend
  sound and reanchor the clock without recreating a healthy output. Three total open attempts per configuration/control
  reset, one-second failure backoff, and truthful unavailable status prevent busy-loop
  retries. New session or routing/mute change permits a new bounded attempt budget.
- Application queues do not remove TCP head-of-line blocking, especially during large
  video access-unit writes. Combined-load measurements remain acceptance work.

## Output implementations and boundaries

Core owns PCM types/validation, `IAudioOutput`, worker queue/gain/retry semantics,
playout observations, and bounded video coalescing. It includes no Android, OpenVR,
Windows, or PulseAudio types. Network owns socket mechanics and independent audio
handling. The VR application composition root coordinates session changes/settings.

**Windows:** WASAPI shared mode, 48 kHz PCM with per-stream engine conversion,
PhoneCast-only PCM gain, and a non-ducking media session. No exclusive mode, default
endpoint mutation, or game/system-volume changes. Default-device changes are polled
on the output worker; invalidation triggers bounded reopen. Engine padding is limited
before writes and padding/stream latency provides a playout estimate.

**Steam Frame:** runtime-loaded `libpulse.so.0`, asynchronous/nonblocking mainloop
iteration and writes, a bounded two-second open deadline, requested 20 ms target
buffering and 100 ms maximum stream buffer, and observed latency diagnostics. A sink
exceeding the 100 ms latency budget fails audio rather than accumulating sound.
Teardown disconnects, never drains. Missing loader/server/sink is non-fatal to video.
Builds require PulseAudio headers (`libpulse-dev`); the executable does not directly
link libpulse, and no daemon/configuration is installed or modified.

A read-only SSH probe confirmed AArch64, PipeWire 1.6.8's PulseAudio-compatible server,
`libpulse.so.0`, and a 48 kHz stereo default speaker sink on the target Frame. This is
**environment evidence only**: launch-runtime socket access, headset audibility,
route changes, game mixing, and sleep/wake remain physical tests.

## A/V timing

The recorder timestamps each block's first sample frame using `AudioRecord` frame
position and `TIMEBASE_MONOTONIC`. Partial reads accumulate into full blocks. If the
API cannot supply timestamps, a read-completion-minus-block-duration approximation
is used and diagnosed. No absolute phone-minus-PC clock subtraction is presented as
latency.

Decoded `VideoFrame` now retains actual picture PTS from the MF output sample and
V4L2 capture-buffer timestamp, not the most recent input PTS. Generated/no-timestamp
frames use zero. Android video/audio clock-domain compatibility still needs hardware
verification; units alone do not establish a common epoch.

The worker estimates audible media time from the end of the last submitted audio
block minus observed output latency. A monotonic steady-clock mapping interpolates
only through submitted samples, bounds each observation correction to 2 ms, and
expires after 100 ms without a valid observation. Session/control reset, device
failure, and large forward packet gaps invalidate that mapping. The VR loop retains at most eight decoded
frames (enough for a 100 ms wait at 30/60 FPS) and picks/coalesces those matching
that clock with 10 ms tolerance. It never waits on a sound device on the render
thread. Each picture has an independent 100 ms arrival-based deadline. When audio
lags farther behind, only already-due pictures are released/coalesced; future
pictures retain their original deadlines, maintaining continuous video cadence.
Unexpected capacity overflow also forces progress rather than renewing the wait.
Unavailable/stale clock, unknown PTS, or a domain discrepancy above two seconds
uses immediate newest-picture video-only presentation. Severe audio discontinuities flush/reanchor rather than build seconds
of lag. Output underflow naturally produces silence; there is no codec-state recovery
or video keyframe request for audio loss.

This is an initial bounded synchronization policy, **not measured lip-sync accuracy
or photon-level latency**. Clock skew/rate correction and timing thresholds require
the physical click/flash test below; the current policy reanchors rather than claiming
continuous calibrated drift compensation.

## Audio-enabled video cadence follow-up — 2026-10-05

The owner reported decent audio but much worse video, improving after restarting
without audio. Read-only Frame log/CSV inspection found 29.5 decoded FPS but 5.2
submitted FPS with audio on, versus 29.8 decoded/27.1 submitted FPS afterward
without audio. Dynamic comparison windows contained no transport drops/resyncs
or sampled OpenVR dropped/mispresented frames; audio recorded zero failures.
Output latency averaged 62 ms and the worker queue averaged 6.39 ten-ms blocks.
These indicate buffering, not a directly measured audio/video PTS difference.
Raw snapshots/comparison remain ignored under
`out/diagnostics/frame-audio-20261005-2157/`.

The original three-picture queue could evict a picture before its 100 ms timeout,
continually renewing the oldest arrival time. Its timeout also jumped to newest
and cleared future pictures, throttling presentation even when it fired. A
synthetic 30 FPS source with a 130 ms lagging audio clock reproduced starvation:
zero submissions in ten seconds versus 303 with audio off. The corrected queue
produces 300/303 in that same diagnostic; an exact 30 FPS regression produces
297/300 with the three end-of-run pictures still pending, a 34 ms maximum
steady-state gap, and no coalescing/overflow. These are software simulations,
not a claimed headset FPS or physical lip-sync measurement.

The correction keeps the 100 ms per-picture wait bound, uses eight slots as a
hard memory ceiling (normally around three/four pending at 30 FPS), and releases
only due pictures. There is no increase to audio buffering, no sender/protocol
change, and no change to the Vulkan renderer or headset audio services. At audio
lags beyond that wait budget, smooth video takes precedence over exact sync.

Console/CSV now include `video_sync_queued`, cumulative
`video_sync_deadline_releases`, `video_sync_overflow_releases`,
`video_sync_coalesced`, `video_sync_last_hold_ms`, `video_sync_skew_valid`, and
`video_sync_estimated_skew_ms`. Hold/skew describe the last dequeued picture,
not a one-second average. Positive estimated skew means picture PTS is ahead of
estimated audible media time. The validity flag means nonzero PTS within the
clock-domain guard, not independently verified clock compatibility. This is
neither photon-level latency nor measured audible lip-sync. Clear/session reset
invalidates the timing observation but retains lifetime release/coalescing counters.

Windows all 12 CTest tests and the ARM64 build/QEMU portable/audio/runtime tests
pass. Regression coverage includes continuous 20/30/60 FPS at 20/60/130/250/1000 ms
audio lag, 240 FPS capacity pressure, per-picture deadlines, clock stalls/jitter/
transitions, audio-off/reset, clock mismatch, unknown PTS, and tolerance overflow.
The rebuilt Sprint 13 Frame archive is served by the existing download routes;
the current Sprint 13 Android APK does not need reinstalling for this receiver-only
correction. Quit PhoneCast before updating. Physical retest of smooth audio-enabled
video, skew, memory, and game coexistence remains pending; Sprint 13 stays open.

## Second timing correction — review branch implementation

Branch: `review/sprint-13-av-timing`. Commit `fece424` preserves the previously
uncommitted Sprint 13 implementation and first cadence correction. This receiver-only
follow-up does not change Android, PCM framing, Vulkan, or headset services.

- Video dispatch now checks a portable four-message/8 ms cooperative budget before
  each transport pop and yields immediately after a decoded picture. Every produced
  picture enters playout; it is no longer silently overwritten by the next decode.
  Compressed prediction packets are left queued in order, not arbitrarily discarded.
  One decoder/start/control call can exceed the soft budget.
- Audio dispatch is limited to twelve messages per pass; pointer forwarding to 32
  events per pass. Remaining work is retained for the next event loop.
- The bounded audible media-clock mapping above avoids backward observations and
  unlimited extrapolation. It is not proof of Android clock-domain compatibility.
- Audio sequence and PTS gaps are separately counted. Small losses retain the output;
  forward PTS gaps above 100 ms clear application sound and request a worker-side
  backend flush before fresh writes. Flush requests coalesce; no silence is inserted.
  Pulse flush completion has a 100 ms timeout; WASAPI uses Stop/Reset/Start on its
  existing shared session. Failure follows the existing bounded reopen policy.
  Mute, opt-out, session change, and route change retain immediate invalidation.
- Added console/CSV window maxima: `dispatch_max_messages`, `dispatch_max_ms`,
  `presentation_max_gap_ms`, `loop_max_ms`. Presentation gaps include static content
  and idle periods; they are submission intervals, not photon-level measurements.
  Loop maximum includes event/input and media work before diagnostics, not sleep or
  the CSV/logging work itself. Added lifetime `audio_sequence_gaps`,
  `audio_timestamp_gaps`, `audio_reanchors`, `audio_flushes`, `audio_opens`, classified
  stale/overflow/rejected drop counters and `audio_max_open_ms`. Session/control
  queue clears still contribute to total drops, not those three classified counters.
- Tests simulate bursty transport with multiple decoded pictures, non-picture work,
  interaction cost, and cooperative decode budgets; check no silent picture loss,
  order, queue bounds, and regular interaction opportunities. Clock tests cover
  jitter, expiry, submitted-sample caps, timeline reset, and integer saturation.
  Worker tests exercise packet gaps without a stream-recreation storm.

Validation: Windows build/all 12 CTest tests pass; Linux ARM64 build and QEMU
portable/audio/standalone-runtime tests pass. A private Docker Pulse null sink
passes flush, write-after-flush, unavailable server, and reopen checks. Both live download aliases were hash-verified and download-page/bootstrap tests
pass. Package permissions are normalized to owner-writable files/directories, with
executable bits on binaries/scripts. No physical receiver/runtime/audio service
was restarted. The served Sprint 13 bundle was rebuilt:

- archive SHA-256: `1577b549079fcf547c72d9be0a31a048c96fbe1072ef7fa99353e4726f0e4d09`;
- packaged receiver SHA-256: `f5d4904a7effb79a37dfa940129d782f6894000e020f7aa63fd0d6f1810dae32`.

Next: physical audio-off/on retest with scrolling, notification and rotation load,
then click/flash sync and game mixing. Compare new gap/dispatch/reset counters,
FPS, memory, and owner feedback. Stable video fallback scheduling/hysteresis when
sound timing is unreliable remains further tuning work; the existing immediate
video-only fallback and 100 ms per-picture ceiling are unchanged. The synthetic
bursty test is not the actual V4L2/OpenVR path and does not claim smooth headset
cadence or measured lip-sync. Sprint 13 remains open.

## Automated validation

- Windows x64 build and all 12 CTest tests pass.
- MF sparse-frame regression additionally verifies original decoded picture PTS.
- C++ audio tests cover golden wire bytes, malformed format/metadata/length/overflow,
  mute/gain/opt-out, queue bounds/stale drops, duplicate/epoch rejection, bounded
  output retries, video timing/fallback, settings persistence/migration/cancel, paired
  negotiation, empty capability fallback, and video/keyframe isolation.
- Android JVM tests, debug APK, and lint pass. Audio wire/timestamp tests and actual
  loopback `NetworkStreamer` tests cover old-receiver video-only fallback and negotiated
  config/PCM/opt-out while video remains usable. Recorder and OS permissions require
  physical tests rather than JVM framework stubs.
- Linux ARM64 receiver cross-build and portable/audio/standalone tests pass under QEMU.
- A private x64 Docker PulseAudio **null sink** exercised dynamic loading, silence
  writes, bounded buffering, missing server/invalid sink errors, teardown and reopen.
  It did not play to physical hardware or validate Frame/PipeWire audibility.

Build/runtime procedures are in `development.md`. Initial audibility is user-reported;
the complete hardware acceptance matrix is not approved.

## Physical acceptance matrix (all pending)

Record device/OS, source app/version, output route/backend, requested/actual buffering,
audible startup, audio drops/underruns, signed A/V skew, CPU/memory, combined bandwidth,
and VR timing. Do not retain sample payloads by default.

1. Default-off cast requests no audio permission and sends no audio. Permission deny,
   opt-out, permission revocation, projection revoke/phone lock, and fresh capture.
2. Eligible playback on Windows VR output and native Frame output while game audio
   remains audible; no ducking, default-device replacement, or exclusivity.
3. Capture-blocked/protected app and idle playback: healthy video/input and truthful
   silence status, no microphone/call capture or policy bypass.
4. Mute/unmute, volume/Apply/Cancel/persistence, explicit/system/VR routing, unavailable
   output device/server, default-device change, and bounded recovery.
5. Audio + dynamic video + notification + touch load on VRLink and standalone games,
   both launch orders, compared with audio-off diagnostics.
6. External click/flash or beep/video test at startup and for at least ten minutes:
   measure A/V skew/delay distributions. Provisional steady-state goal: absolute skew
   within 80 ms, subject to actual measurements and owner approval.
7. Repeated rotation, reconnect, old receiver, Stop/Quit, headset sleep/wake and app
   switching; no stale replay, stuck worker, or unbounded queue.
8. Downloads/congestion/stalls and existing decoder/GPU faults: log separately. Audio
   cannot guarantee game survival during a global GPU reset or close Sprint 12 risks.

## Primary sources

- [Android playback capture](https://developer.android.com/media/platform/av-capture)
- [AudioRecord timestamps](https://developer.android.com/reference/android/media/AudioRecord)
- [AudioTimestamp](https://developer.android.com/reference/android/media/AudioTimestamp)
- [Android supported formats](https://developer.android.com/media/platform/supported-formats)
- [Foreground service types](https://developer.android.com/develop/background-work/services/fgs/service-types)
- [WASAPI](https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi)
- [IAudioClient initialization](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-initialize)
- [PulseAudio buffers](https://www.freedesktop.org/software/pulseaudio/doxygen/structpa__buffer__attr.html)
- [PipeWire Pulse compatibility](https://pipewire.pages.freedesktop.org/pipewire/page_man_pipewire-pulse_1.html)
- [Pinned OpenVR audio property](https://github.com/ValveSoftware/openvr/blob/0924064316de3effbcd1acf1e309182a2deb1c05/headers/openvr.h)
