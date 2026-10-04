# VR interaction and Android remote control

Last reviewed: 2026-10-03

Sprints: 7 and 8 — implementation complete; physical phone/headset validation pending

## Selected Android integration

PhoneCast uses an explicitly user-enabled `AccessibilityService` for touch gestures and Android Back. This is the only public, non-root Android API evaluated that can inject cross-application taps and swipes on ordinary consumer devices.

It is deliberately optional and has two independent gates:

1. the user checks **Allow remote control while casting** in PhoneCast;
2. the user enables **PhoneCast remote control** in Android Accessibility settings.

The service declares `canPerformGestures`, does not retrieve window content, and does not inspect accessibility nodes, app text, or typed text. It handles only commands received over the same phone-initiated, paired streaming connection. The app shows a prominent disclosure before directing the user to Accessibility settings.

Android's accessibility API is intended for accessibility use. A future Play-distributed release that is not an accessibility tool must satisfy the then-current Google Play declaration, prominent-disclosure, consent, and permitted-use policy. This implementation is technically suitable for development and informed side-loaded use; successful store review is not implied.

Primary references:

- [Create your own accessibility service](https://developer.android.com/guide/topics/ui/accessibility/service)
- [`AccessibilityService.dispatchGesture`](https://developer.android.com/reference/android/accessibilityservice/AccessibilityService#dispatchGesture(android.accessibilityservice.GestureDescription,%20android.accessibilityservice.AccessibilityService.GestureResultCallback,%20android.os.Handler))
- [`AccessibilityService.performGlobalAction`](https://developer.android.com/reference/android/accessibilityservice/AccessibilityService#performGlobalAction(int))
- [Google Play AccessibilityService API policy](https://support.google.com/googleplay/android-developer/answer/10964491)

## Alternatives considered

- **ADB / `adb shell input`:** capable, but requires Developer Options and an authorized debugging host. It is retained as a development alternative, not the normal product path.
- **scrcpy-style control:** scrcpy starts an on-device server through ADB and uses hidden/privileged input injection or UHID. Reusing that model would silently introduce developer-mode or privileged assumptions. USB AOA/HID is not suitable for the intended Wi-Fi-only standalone architecture.
- **Hidden `InputManager.injectInputEvent`:** requires signature-level `INJECT_EVENTS` privileges and is not available to an ordinary installed application.
- **Device owner:** device-policy APIs can manage dedicated devices but do not grant general touch-injection privileges.
- **Root / system image / OEM integration:** technically capable but rejected as a normal-product dependency.

No path silently depends on root.

## Portable interaction model

`OverlayInteractionController` converts renderer coordinates to normalized, top-left phone coordinates. OpenVR's bottom-left mouse origin is inverted before an event crosses the VR boundary.

Supported portable events are:

- `Down`
- `Move`
- `Up`
- `Scroll`
- `Back`

The stream protocol carries a fixed 16-byte input payload in a `REMOTE_INPUT` message. Coordinates and scroll deltas are bounded and validated on both endpoints. The event sequence is carried in the common message header. OpenVR types, Android types, and pixel dimensions do not enter the portable event contract.

## VR controls

- Point the controller laser at the phone overlay.
- Press and release trigger for a tap.
- Hold trigger, move, and release for a swipe or long press.
- Runtime overlay scroll events map to Android scroll gestures.
- Select **Back** from the PhoneCast dashboard for Android Back.
- Hold grip while pressing trigger to grab/reposition the overlay; trigger without grip is reserved for phone interaction.

The Android service buffers the normalized Down/Move/Up path and submits one gesture on Up. Android's `dispatchGesture` API submits complete gestures and cancels an already-dispatched gesture, so streaming partial gesture segments would be less reliable. This means the phone receives the swipe when trigger is released rather than seeing a continuously injected finger.

## Security and privacy

Remote input travels receiver-to-phone over the existing duplex TCP connection only after the phone initiated the connection and completed the six-digit pairing handshake. The Android opt-in and Accessibility-service enablement prevent remote control by default.

The transport is still unencrypted and the six-digit code is not cryptographic authentication. A capable attacker on the trusted LAN could observe or modify video and input traffic. Remote control must remain disabled on untrusted networks. Authenticated encryption and durable device identity remain required before normal product use.

Input logs contain event categories/status only; they do not log coordinates, screen content, app text, or typed text.

## Validation status

Automated validation completed:

- portable coordinate conversion and Y-axis inversion;
- stable event sequencing;
- C++ and Java remote-input payload parsing and bounds validation;
- Windows x64 clean configure/build and all CTest tests;
- Android JVM tests, debug APK assembly, and lint.

Still requires physical validation:

- Accessibility disclosure/enable/disable flow on a phone;
- tap, long press, swipe, scroll, and Back in ordinary Android applications;
- portrait/landscape coordinate accuracy;
- controller laser and scroll delivery through SteamVR/VRLink;
- overlay grip+trigger placement after trigger interaction changed;
- reconnect behavior and disabling control during an active stream;
- behavior over a running VR game and native Steam Frame backend.

Do not mark Sprints 7 or 8 physically complete until those checks pass.
