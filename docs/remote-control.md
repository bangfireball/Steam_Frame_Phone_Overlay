# VR interaction and Android remote control

Last reviewed: 2026-10-03

Sprints: 7 and 8 — complete and physically approved for the PC-hosted Windows/VRLink path

## Selected Android integration

PhoneCast uses an explicitly user-enabled `AccessibilityService` for touch gestures and Android Back. This is the only public, non-root Android API evaluated that can inject cross-application taps and swipes on ordinary consumer devices.

It is deliberately optional and has two independent gates:

1. the user checks **Allow remote control while casting** in PhoneCast;
2. the user enables **PhoneCast remote control** in Android Accessibility settings.

The service declares `canPerformGestures`, does not retrieve window content, and does not inspect accessibility nodes, app text, or typed text. It handles only commands received over the same phone-initiated, paired streaming connection. The app shows a prominent disclosure before directing the user to Accessibility settings.

Android reports the two gate states independently to the authenticated receiver when a connection starts and whenever consent or Accessibility connectivity changes. The receiver logs a specific warning and shows **Enable app control**, **Enable Accessibility**, or **Enable both control gates** in the PhoneCast dashboard until both are open. This status is advisory; Android still enforces both gates for every command.

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
- Select **Back** from the PhoneCast dashboard or use the translucent `<` button at the lower-left edge of the phone overlay.
- Point at the horizontal handle below the phone, hold trigger, move, and release to reposition the overlay. The handle is outside the Android touch surface, so trigger gestures on the phone remain unambiguous.

For a tap or stationary long press, Android receives one complete gesture on trigger release. Once a held pointer starts moving, the Accessibility service uses `StrokeDescription.continueStroke` to dispatch serialized gesture segments while trigger remains held. This permits live drag-scrolling instead of waiting until release; the final segment releases the injected finger.

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

Physically confirmed on the Windows/VRLink path:

- Accessibility service enablement and the separate in-app opt-in gate;
- tap, long press, four-direction swipe, runtime scroll, and smooth held-trigger drag scrolling;
- corner-coordinate accuracy in portrait and landscape;
- dashboard and lower-left overlay Back controls;
- bottom-handle placement and stable world-lock release in portrait and landscape;
- reconnect, disabling either Android gate during an active stream, and restoration after re-enabling both gates.

The first running-game test found that leaving OpenVR's `MakeOverlaysInteractiveIfVisible` flag on the persistent phone surface activated SteamVR's system-wide laser and withheld controller poses/input from the game whenever the phone was visible. Removing that flag fixed the regression. Physical retesting confirmed that the phone remains visible and continues updating—including video playback—while a game runs, game hands remain active with the dashboard closed, dashboard-open phone interaction still works, and closing the dashboard immediately restores input priority to the game.

Controller/hand-locked placement can visibly jitter while the overlay is moved across the view. The project owner accepted this as non-blocking for Sprints 7–8; it remains a placement-quality follow-up rather than a claim of jitter-free tracking.

Sprints 7 and 8 were approved for the PC-hosted path after these checks. Native Steam Frame backend behavior remains separately owned by Sprint 11.
