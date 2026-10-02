# PhoneCast streaming protocol

Last reviewed: 2026-10-02

Sprint 3 status: **implementation in progress; physical-device streaming validation pending**

## Transport decision

Sprint 3 uses one persistent, low-delay TCP connection initiated by the Android sender. The receiver listens on TCP port `49321` by default and enables `TCP_NODELAY`.

This is an intentionally narrow first implementation:

- TCP is available in the Android SDK, Windows, and Linux ARM64 without a large runtime dependency.
- Reliable ordered delivery avoids writing H.264 fragmentation, reassembly, jitter buffering, and loss recovery before the basic pipeline has been validated.
- A three-frame sender queue and four-message receiver queue bound latency and memory. Old non-config frames are discarded when either consumer falls behind.
- Reconnection uses exponential backoff capped at five seconds. A reconnect sends the pairing handshake and latest codec configuration, then requests a fresh keyframe.

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

`VIDEO_FRAME` flag bit 0 identifies a keyframe. Android output buffers are copied only from `BufferInfo.offset` through `offset + size`. Codec-config output is not counted as a video frame.

## Session behavior

1. Receiver starts with an operator-selected six-digit pairing code.
2. Android connects to the manually entered receiver address.
3. Android sends `HELLO`; the receiver closes the connection if the code does not match.
4. Android sends the latest `VIDEO_CONFIG` before frames.
5. Keyframes are prefixed with the current codec configuration before being submitted to the Windows decoder.
6. An orientation/configuration change clears queued old frames, sends new configuration and dimensions, and resets the decoder.
7. A disconnect leaves capture active while the sender retries with bounded exponential backoff.

Manual address entry satisfies Sprint 3's “discovers or connects” criterion. Automatic discovery is deferred until the basic stream has physical-device validation.

## Decoder decision

The Sprint 3 PC preview uses the Windows Media Foundation H.264 decoder MFT behind a platform-specific class. MediaCodec's Annex-B SPS/PPS are prepended to keyframes, matching the decoder's documented Annex-B input requirement. Core protocol and frame types contain no Windows headers. This avoids adding a large codec distribution solely for the PC feasibility stage.

Steam Frame will require a Linux ARM64 decoder backend. FFmpeg/libavcodec remains a candidate because its send/receive API and ARM64 support are portable, but selecting the native Steam Frame hardware path requires device investigation and is not implied by the Windows implementation.

## Security status

The receiver accepts only an outbound connection initiated by a user-configured Android sender and requires the displayed six-digit code before accepting video. This prevents an unauthenticated LAN client from injecting video into the receiver.

**The Sprint 3 protocol is not encrypted and the short code is not a cryptographic authentication protocol. Screen content can be observed by an attacker able to capture LAN traffic, and an active attacker could impersonate an endpoint. Do not use it on an untrusted network.**

Sprint 3 must not be represented as security-complete. Before normal product use, replace this bootstrap with authenticated encryption and persistent device identity (for example TLS with certificate pinning established through a QR/fingerprint pairing flow), plus device revocation.

## Diagnostics and limitations

The receiver window reports decoded FPS, received bitrate, average decode time, dimensions, queue/sequence drops, and connection state. The Android sender reports network RTT using a once-per-second `PING`/`PONG` exchange. Android presentation timestamps cannot be directly subtracted from the PC clock; meaningful glass-to-glass latency still requires clock-offset estimation or an external high-speed-camera test.
