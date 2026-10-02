# PhoneCast streaming protocol

Last reviewed: 2026-10-02

Sprint 3 status: **physical Android-to-PC streaming confirmed; performance and rendering fixes remain**

## Transport decision

Sprint 3 uses one persistent, low-delay TCP connection initiated by the Android sender. The receiver listens on TCP port `49321` by default and enables `TCP_NODELAY`.

This is an intentionally narrow first implementation:

- TCP is available in the Android SDK, Windows, and Linux ARM64 without a large runtime dependency.
- Reliable ordered delivery avoids writing H.264 fragmentation, reassembly, jitter buffering, and loss recovery before the basic pipeline has been validated.
- A three-frame sender queue and sixteen-message receiver queue bound latency and memory. The receiver depth absorbs decoder startup and short TCP/scheduler bursts; if either queue still overruns, the implementation discards the dependent frame chain, waits for a new keyframe, and explicitly requests one instead of decoding arbitrary inter-frame gaps.
- Reconnection uses exponential backoff capped at five seconds. A reconnect clears stale queued frames, sends the pairing handshake and latest codec configuration, then requests a fresh keyframe.

The tradeoff is TCP head-of-line blocking after packet loss. RFC 8087 explains that reliable ordered transports delay later data while a missing segment is retransmitted. RTP (RFC 3550) avoids that behavior and provides media sequence/timestamp semantics, while WebRTC adds SRTP, congestion control, feedback, and NAT traversal at substantial integration and binary-size cost.

For the initial same-LAN 30 FPS target, custom framed TCP is the smallest useful feasibility step. If Wi-Fi loss measurements show visible stalls, the next transport evaluation should compare RTP/SRTP and WebRTC rather than adding ad-hoc unreliable delivery.

Primary references:

- [RFC 8087 — UDP Usage Guidelines, TCP latency discussion](https://www.rfc-editor.org/rfc/rfc8087.html)
- [RFC 3550 — RTP](https://www.rfc-editor.org/rfc/rfc3550.html)
- [RFC 8835 — WebRTC transports](https://www.rfc-editor.org/rfc/rfc8835.html)
- [Android MediaCodec](https://developer.android.com/reference/android/media/MediaCodec)
- [FFmpeg send/receive API](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html) (evaluated, not selected for the current Windows decoder)
- [Microsoft H.264 Video Decoder MFT](https://learn.microsoft.com/en-us/windows/win32/medfound/h-264-video-decoder)

## Wire format

Every integer is unsigned and encoded in network byte order. Each message starts with a fixed 32-byte header:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII magic `PCVR` |
| 4 | 1 | protocol version (`1`) |
| 5 | 1 | message type |
| 6 | 2 | flags |
| 8 | 4 | payload length |
| 12 | 8 | sequence number |
| 20 | 8 | encoder presentation timestamp in microseconds |
| 28 | 2 | encoded width |
| 30 | 2 | encoded height |

Payloads are limited to 4 MiB before allocation.

Message types:

1. `HELLO` — six ASCII pairing-code digits; must be first.
2. `VIDEO_CONFIG` — H.264 codec-specific data (SPS/PPS emitted by MediaCodec).
3. `VIDEO_FRAME` — one encoded H.264 access unit.
4. `PING` — sender monotonic timestamp used for RTT diagnostics.
5. `PONG` — receiver echo of the `PING` timestamp.
6. `END_STREAM` — orderly sender shutdown.
7. `REQUEST_KEY_FRAME` — receiver-to-sender recovery feedback with an empty payload.

`VIDEO_FRAME` flag bit 0 identifies a keyframe. Android output buffers are copied only from `BufferInfo.offset` through `offset + size`. Codec-config output is not counted as a video frame.

## Session behavior

1. Receiver starts with an operator-selected six-digit pairing code.
2. Android connects to the manually entered receiver address.
3. Android sends `HELLO`; the receiver closes the connection if the code does not match.
4. Android sends the latest `VIDEO_CONFIG` before frames.
5. Keyframes are prefixed with the current codec configuration before being submitted to the Windows decoder.
6. An orientation/configuration change clears queued old frames, sends new configuration and dimensions, resets the decoder, and requests a keyframe.
7. A sequence gap or queue overrun causes both endpoints to discard stale dependent frames and resume only at a requested keyframe.
8. A disconnect leaves capture active while the sender retries with bounded exponential backoff.

Manual address entry satisfies Sprint 3's “discovers or connects” criterion. Automatic discovery is deferred until the basic stream has physical-device validation.

## Decoder decision

The Sprint 3 PC preview uses the Windows Media Foundation H.264 decoder MFT behind a platform-specific class. MediaCodec's Annex-B SPS/PPS are prepended to keyframes, matching the decoder's documented Annex-B input requirement. Core protocol and frame types contain no Windows headers. This avoids adding a large codec distribution solely for the PC feasibility stage.

Steam Frame will require a Linux ARM64 decoder backend. FFmpeg/libavcodec remains a candidate because its send/receive API and ARM64 support are portable, but selecting the native Steam Frame hardware path requires device investigation and is not implied by the Windows implementation.

## Security status

The receiver accepts only an outbound connection initiated by a user-configured Android sender and requires the displayed six-digit code before accepting video. This prevents an unauthenticated LAN client from injecting video into the receiver.

**The Sprint 3 protocol is not encrypted and the short code is not a cryptographic authentication protocol. Screen content can be observed by an attacker able to capture LAN traffic, and an active attacker could impersonate an endpoint. Do not use it on an untrusted network.**

Sprint 3 must not be represented as security-complete. Before normal product use, replace this bootstrap with authenticated encryption and persistent device identity (for example TLS with certificate pinning established through a QR/fingerprint pairing flow), plus device revocation.

## Physical validation

A physical Android sender connected to the Windows receiver, authenticated, reconnected after a receiver restart, and produced a visible `886 × 1920` phone image. Media Foundation reported an output stream change after reading the H.264 parameter sets; the receiver now renegotiates NV12 output and measured approximately 5.6 ms average decode time during the initial validation.

Known defects from that test and current follow-up status:

- the original right-edge green bar was fixed by separating Media Foundation's aligned coded dimensions from the phone's visible dimensions; physical Sprint 4 validation confirmed the correction;
- portrait display works in-headset, but landscape currently fails;
- playback remains visibly stuttering and has not met the approximately 30 FPS acceptance target;
- the first visible overlay frame takes approximately 10–30 seconds;
- observed latency is currently unusable, although quantitative glass-to-glass latency has not yet been recorded.

## Diagnostics and limitations

The receivers report one-second rolling receive/decode/render FPS, bitrate, decode/render time, queue age/depth, drops, resynchronization requests, and connection state. They also report connection-to-first-decoded-frame time. The Android sender reports network RTT using a once-per-second `PING`/`PONG` exchange. Android presentation timestamps cannot be directly subtracted from the PC clock; meaningful glass-to-glass latency still requires clock-offset estimation or an external high-speed-camera test.

The first physical test of the initial pacing pass showed that the input-surface frame-rate request was only advisory on the test phone: the receiver observed roughly 90–110 encoded frames per second, decoded many frames that were never presented, repeatedly overran/resynchronized, and rendered only roughly 8–16 updates per second. Connection-to-first-submitted-frame was about 15.4 seconds in that run. Decode remained roughly 5–6 ms and steady-state D3D submission roughly 0.5 ms, so neither operation alone explains the defect.

The Android encoder sets `KEY_MAX_FPS_TO_ENCODER` to 30 FPS, which asks MediaCodec to discard excess surface input before H.264 dependencies are formed. Physical validation confirmed steady dynamic periods near 30 received/decoded FPS with no receiver drops. A follow-up attempt to use `KEY_REPEAT_PREVIOUS_FRAME_AFTER` as a 10 FPS static heartbeat was ineffective on the test encoder: diagnostics still showed multi-second periods with no encoded frames, proving the remaining delay occurs before the receiver rather than in its roughly 5–15 ms queue/decode/render path.

The next low-latency sender pass reduces the long edge from 1920 to 1280, reduces keyframe frequency from every one to every two seconds, selects CBR and the codec low-latency feature when advertised, and removes the ineffective surface frame-rate/repeat hints. This reduces keyframe size, TCP send blocking, Wi-Fi contention with VRLink, and encoder workload. The sender also caches the latest complete keyframe so a newly connected receiver can display it immediately while requesting a fresh recovery frame. These changes require physical validation.

The VR overlay displays a waiting texture immediately so visibility controls work before video arrives. Startup diagnostics separately report configuration, keyframe, decoder initialization, and first-submission timing.
