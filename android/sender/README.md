# PhoneCast Android Sender

This Android application captures the user-approved display with `MediaProjection`, feeds it into a surface-input H.264/AVC `MediaCodec` encoder, and streams encoded access units to the native Steam Frame receiver over the local network. The Windows receiver is a secondary supported path. The v0.8 app uses `versionCode` 3.

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

## Signed v0.8 release

Release builds use a private `signing.properties` file outside the repository:

- Default: `%USERPROFILE%\.phonecast-vr\android-release\signing.properties` on Windows, or `~/.phonecast-vr/android-release/signing.properties` elsewhere.
- Override: set `PHONECAST_SIGNING_PROPERTIES` to an absolute path to another private properties file.

The properties file contains these four fields (example placeholders, not actual credentials):

```properties
storeFile=C:/Users/YOUR_USER/.phonecast-vr/android-release/phonecast-vr-release.jks
storePassword=YOUR_PRIVATE_KEYSTORE_PASSWORD
keyAlias=phonecast-vr
keyPassword=YOUR_PRIVATE_KEY_PASSWORD
```

Use an absolute keystore path and forward slashes in properties files. Never commit this file, the keystore, or passwords. If you already have a release signing key, reuse it rather than generating a replacement. The Android debug keystore at `~/.android/debug.keystore` is not a public release key.

To create a key yourself, open this project in Android Studio, choose **Build → Generate Signed Bundle / APK → APK → Create new**, and save the keystore outside the repository. Use a strong password, alias `phonecast-vr`, and validity of at least 25 years. Then create the private properties file above. Restrict filesystem access to your account and store an encrypted backup of both the keystore and its credentials. APK certificates expose their certificate identity publicly; do not put private personal details in that identity.

Build from `android/sender`:

```powershell
.\gradlew.bat testDebugUnitTest assembleDebug lintDebug assembleRelease lintRelease
```

Signed APK:

```text
app/build/outputs/apk/release/app-release.apk
```

Release assembly fails when signing configuration is missing; debug builds/tests still work without it. Verify the signed APK with your Android SDK build-tools `apksigner.bat`:

```powershell
apksigner.bat verify --verbose --print-certs app/build/outputs/apk/release/app-release.apk
```

Publish only the verified APK, never the signing material. For GitHub, the APK can be copied/renamed to `phonecast-vr-v0.8-android.apk`.

**Debug-to-release migration:** a release-signed APK cannot update a debug-signed installation. Stop casting first so brightness and phone volume can restore, note your settings, uninstall the debug app, then install the release APK and grant optional permissions again. Uninstalling clears saved connection/settings data. Subsequent public updates must use this same release key and a higher `versionCode`; losing the key or password prevents ordinary in-place updates for GitHub APK users.

See [Android app-signing documentation](https://developer.android.com/studio/publish/app-signing). APK signing establishes publisher/update identity; it does not encrypt PhoneCast's network stream.

## Install

Enable USB or wireless debugging, connect the phone, then:

```powershell
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Start `phonecast-stream-receiver.exe --pair-code 123456` for desktop video or `phonecast-vr-stream-receiver.exe --pair-code 123456` for VR on the PC. Open **PhoneCast**, enter the PC's LAN IPv4 address and matching code, press **Start casting**, approve notification permission when requested, and approve Android's screen-sharing dialog. The address and code are retained so normal startup takes only the Start action and Android's required screen-sharing approval. After saving valid receiver details, add the **PhoneCast Quick connect** widget to the Android home screen for a one-tap route directly to the required screen-sharing prompt. It supports a compact 1 × 1 icon and reveals its label when expanded. The widget never embeds or displays the pairing code and cannot bypass per-session MediaProjection consent. If no valid details are saved, it opens PhoneCast with an explanatory status instead. The app displays encoded frame, byte, and congestion-drop counters. Use the in-app button or foreground notification action to stop.

Optional features are under the main screen's menu button so the launch screen remains focused on connection and Start/Stop. Remote control is optional. Read the in-app disclosure, check **Allow remote control while casting**, open Accessibility settings, and explicitly enable **PhoneCast remote control**. Casting does not require remote control. The service can inject user-requested taps, swipes, scrolls, and Back, but is configured not to retrieve window content. Disable either gate to stop control. See `docs/remote-control.md` for alternatives, Google Play policy implications, and security limits.

**Dim phone while casting** is enabled by default and can be turned off in Settings. On first use, Android requires separate **Modify system settings** approval before PhoneCast can change device-wide brightness. If access is declined, casting continues undimmed. The default 15-second delay can be changed to 5, 30, or 60 seconds. Settings and the casting notification provide **Restore brightness**; when the optional PhoneCast Accessibility service is enabled, a physical touch also attempts temporary restoration without collecting touch coordinates. PhoneCast restores the prior brightness mode on reachable stop paths and records recovery state before changing it. Force Stop cannot run immediate cleanup; reopen PhoneCast or use Android Quick Settings if recovery is needed. This is minimum-brightness operation, not panel-off or lock-screen capture.

Notification forwarding is also optional and off by default. Enable **Forward notifications to VR while casting**, grant Android notification access, and optionally enter comma-separated package allow/block lists. Selecting a VR card reveals the phone and invokes the original notification tap action when the source supplied one and it remains valid. Cards hide titles and message text unless **Include notification titles and message text (sensitive)** is explicitly enabled. Notification data is sent only through the active casting connection. See `docs/notifications.md` for the privacy model and physical-validation checklist.

Useful diagnostics:

```powershell
adb logcat -s PhoneCastCapture
```

## Current behavior

- Provides persisted Battery Saver (960/20 FPS), Standard (1280/30 FPS), and Quality (1920/30 FPS) streaming profiles; values are long-edge caps.
- Preserves aspect ratio and uses Standard by default to retain the physically accepted latency-oriented path.
- Uses hardware H.264 when the device's default AVC encoder is hardware-backed; Android selects the encoder.
- Runs capture from a `mediaProjection` foreground service.
- Drains encoded output continuously and reports frame/byte counts.
- Sends versioned, size-bounded messages over a low-delay duplex TCP connection.
- Optionally accepts normalized remote input from the authenticated receiver and forwards it to the separately enabled Accessibility service.
- Optionally forwards privacy-filtered, allow/block-listed notification cards over the active stream connection.
- Keeps only a few pending frames so a slow network does not stall the encoder indefinitely.
- Reconnects with bounded exponential backoff and requests a fresh keyframe.
- Replaces the encoder surface and resizes the existing virtual display when captured content changes size.
- Stops cleanly when the user revokes sharing, locks the device on Android versions that revoke projection, presses Stop, or uses the notification action.
- Refreshes the main Start/Stop state when the activity resumes, so an externally ended projection does not leave stale controls.
- Reports the in-app remote-control consent and Accessibility-service gate independently to the receiver.
- Provides a home-screen Quick connect widget that reuses the saved receiver address, pairing code, and streaming profile before showing Android's mandatory capture-consent prompt.
- Defaults to opt-out phone dimming after 15 seconds when Modify system settings access is granted, with durable restoration state and explicit temporary-restore controls.

## Optional phone playback audio

Settings → **Phone playback audio** adds a separately opt-in, default-off toggle.
Grant Android audio-recording permission only if you want this feature, then start
a new cast. It captures permitted media/game playback using the existing projection,
not microphones or calls. Some apps return silence or prohibit capture; PhoneCast
will not bypass this. The phone may continue playing locally because capture copies
rather than redirects sound.

The separate **Mute phone while streaming audio** setting saves the current global
media volume before muting local playback. It restores that exact value on receiver
disconnect, audio opt-out, projection/casting stop, or service teardown. PhoneCast
commits a recovery marker before muting; after an unclean process exit, the next
PhoneCast process start attempts restoration before another component starts.
Android cannot execute restoration at the instant its process is forcibly killed,
so that last recovery is delayed until PhoneCast runs again. The setting changes
media volume only, never call/ring volume. Physical testing confirms the target phone
continues supplying playback capture while local media volume is zero; individual
restore/failure paths remain to be exercised.

Source-app UI mute is not reliably visible to playback capture. In physical testing,
Reddit produced captured audio for some embedded videos shown as muted. PhoneCast
receives PCM selected by Android usage/capture policy and cannot infer that per-video
UI state. Disabling phone playback audio stops it; a future package-level blocklist
could exclude an application entirely.

Turning the playback-audio toggle off stops current audio immediately; enabling
applies to the next capture session. Video continues on older receivers and on
handled audio failures. Supported native playback, perceived sync, game mixing,
local mute, controls, and sustained playback are owner-approved. See
`docs/phone-audio.md` for closure evidence, deferred cases, and receiver controls.

## Security limitation

The receiver requires the six-digit code before accepting video, but the current Sprint 3 transport is not encrypted. Use it only on a trusted development LAN. See `docs/protocol.md` for the protocol decision and required security follow-up.
