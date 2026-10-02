# PhoneCast Android Sender — Sprint 2

This Android application captures the user-approved display with `MediaProjection` and feeds it directly into a surface-input H.264/AVC `MediaCodec` encoder. Encoded output is drained and measured, but intentionally discarded: networking begins in Sprint 3.

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

Open **PhoneCast Sender**, press **Start capture**, approve notification permission when requested, and approve Android's screen-sharing dialog. The app displays encoded frame and byte counters. Use the in-app button or foreground notification action to stop.

Useful diagnostics:

```powershell
adb logcat -s PhoneCastCapture
```

## Current behavior

- Captures at 30 FPS.
- Preserves aspect ratio and caps the longest encoded edge at 1920 pixels.
- Uses hardware H.264 when the device's default AVC encoder is hardware-backed; Android selects the encoder.
- Runs capture from a `mediaProjection` foreground service.
- Drains encoded output continuously and reports frame/byte counts.
- Replaces the encoder surface and resizes the existing virtual display when captured content changes size.
- Stops cleanly when the user revokes sharing, presses Stop, or uses the notification action.

## Sprint boundary

No network connection is opened and no screen bytes leave the phone. The encoded access units become Sprint 3's transport input.
