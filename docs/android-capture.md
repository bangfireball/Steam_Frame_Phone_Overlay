# Android screen capture and encoding

Last reviewed: 2026-10-02

Sprint: 2 — Android Screen Capture

## Documented requirements

Android's MediaProjection documentation requires explicit user consent for each capture session. For applications targeting Android 14 or newer, a consent token may be used for only one `MediaProjection.createVirtualDisplay` call.

The capture must run in a foreground service declared with type `mediaProjection`. The manifest requires both `FOREGROUND_SERVICE` and `FOREGROUND_SERVICE_MEDIA_PROJECTION`. The service must enter the foreground before calling `MediaProjectionManager.getMediaProjection`.

A `MediaProjection.Callback` must be registered before creating the virtual display. `onStop` is the authoritative signal to release the virtual display, encoder surface, codec, and projection.

A surface-input `MediaCodec` AVC encoder allows the virtual display to render into the encoder without an application-level pixel readback. Encoded output buffers must be drained continuously.

For captured-content resize on Android 14+, the existing virtual display should be resized and assigned an appropriately sized surface rather than creating a second virtual display from the same consent token.

Primary sources:

- [Android Developers — Media projection](https://developer.android.com/media/grow/media-projection)
- [Android Developers — MediaProjection](https://developer.android.com/reference/android/media/projection/MediaProjection)
- [Android Developers — MediaCodec](https://developer.android.com/reference/android/media/MediaCodec)
- [Android Developers — Foreground service types](https://developer.android.com/about/versions/14/changes/fgs-types-required)

## Implemented behavior

The Sprint 2 sender:

1. Requests screen-capture consent from `MainActivity`.
2. Starts `ScreenCaptureService` from the user action.
3. Enters the foreground with the `mediaProjection` service type.
4. Obtains and registers the approved projection.
5. Selects dimensions preserving aspect ratio using the persisted streaming profile: Battery Saver caps the long edge at 960 pixels, Standard at 1280, and Quality at 1920.
6. Configures a surface-input AVC encoder at 20 FPS for Battery Saver or 30 FPS for Standard/Quality, with profile-specific bitrate bounds.
7. Creates one virtual display targeting the encoder surface.
8. Drains H.264 output, counts frames and bytes, and discards payloads.
9. On resize, detaches the old surface, replaces the encoder, resizes the same virtual display, and attaches the replacement surface.
10. Releases all resources on stop or projection revocation.
11. Persists the authoritative running flag and refreshes the launch-screen action when the activity resumes, so a screen-lock or other external projection revocation returns the UI to **Start casting** without a manual Stop cycle.
12. Offers an Android home-screen Quick connect widget after valid receiver details have been saved. The widget opens the activity with an explicit app-private action, reloads the saved address/code/profile, and proceeds directly to Android's required MediaProjection consent. It does not store credentials in the widget or bypass consent.

The app logs no screen content, encoded payloads, or sensitive text.

## Validation status

**Result:** Sprint 2 passed on 2026-10-02.

- Debug APK compilation: passed.
- JVM configuration tests: passed.
- Android lint: passed.
- User granted MediaProjection permission and the service entered its capturing/encoding state on a physical Android device.
- Encoded frame counters increased continuously, confirming drained H.264 output.
- Portrait/landscape rotation continued working.
- Stop, start, and restart behavior worked.

The exact device model, Android version, encoder identity, sustained FPS, and bitrate were not recorded, so compatibility and performance across other devices remain unmeasured. Standard preserves the physically accepted Sprint 4 path. Battery Saver and Quality are Sprint 10 measurement candidates and must not be represented as physically approved until the procedure in `docs/performance.md` is completed.
