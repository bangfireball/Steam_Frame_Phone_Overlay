# PhoneCast Android Sender

This Android application captures the user-approved display with `MediaProjection`, feeds it into a surface-input H.264/AVC `MediaCodec` encoder, and streams encoded access units to the Sprint 3 PC receiver over the local network.

## Requirements

- Android 10 (API 29) or newer
- Android SDK 36
- JDK 17 or newer
- A device with an AVC surface-input encoder

## Build and test

From `android/sender`:

```powershell
.\gradlew.bat testDebugUnitTest assembleDebug lintDebug
```

APK:

```text
app/build/outputs/apk/debug/app-debug.apk
```

## Install

Enable USB or wireless debugging, connect the phone, then:

```powershell
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Start `phonecast-stream-receiver.exe --pair-code 123456` for desktop video or `phonecast-vr-stream-receiver.exe --pair-code 123456` for VR on the PC. Open **PhoneCast Sender**, enter the PC's LAN IPv4 address and matching code, press **Start casting**, approve notification permission when requested, and approve Android's screen-sharing dialog. The app displays encoded frame, byte, and congestion-drop counters. Use the in-app button or foreground notification action to stop.

Remote control is optional. Read the in-app disclosure, check **Allow remote control while casting**, open Accessibility settings, and explicitly enable **PhoneCast remote control**. Casting does not require remote control. The service can inject user-requested taps, swipes, scrolls, and Back, but is configured not to retrieve window content. Disable either gate to stop control. See `docs/remote-control.md` for alternatives, Google Play policy implications, and security limits.

Notification forwarding is also optional and off by default. Enable **Forward notifications to VR while casting**, grant Android notification access, and optionally enter comma-separated package allow/block lists. Cards hide titles and message text unless **Include notification titles and message text (sensitive)** is explicitly enabled. Notification data is sent only through the active casting connection. See `docs/notifications.md` for the privacy model and physical-validation checklist.

Useful diagnostics:

```powershell
adb logcat -s PhoneCastCapture
```

## Current behavior

- Captures at 30 FPS.
- Preserves aspect ratio and currently caps the longest encoded edge at 1280 pixels to prioritize latency.
- Uses hardware H.264 when the device's default AVC encoder is hardware-backed; Android selects the encoder.
- Runs capture from a `mediaProjection` foreground service.
- Drains encoded output continuously and reports frame/byte counts.
- Sends versioned, size-bounded messages over a low-delay duplex TCP connection.
- Optionally accepts normalized remote input from the authenticated receiver and forwards it to the separately enabled Accessibility service.
- Optionally forwards privacy-filtered, allow/block-listed notification cards over the active stream connection.
- Keeps only a few pending frames so a slow network does not stall the encoder indefinitely.
- Reconnects with bounded exponential backoff and requests a fresh keyframe.
- Replaces the encoder surface and resizes the existing virtual display when captured content changes size.
- Stops cleanly when the user revokes sharing, presses Stop, or uses the notification action.

## Security limitation

The receiver requires the six-digit code before accepting video, but the current Sprint 3 transport is not encrypted. Use it only on a trusted development LAN. See `docs/protocol.md` for the protocol decision and required security follow-up.
