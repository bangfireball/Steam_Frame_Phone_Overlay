# Android Screen-Off Behavior During Phone Casting

## Purpose

This document records whether PhoneCast can continue capturing an Android phone while its physical display is off, and how Samsung Link to Windows / Microsoft Phone Link handles this experience.

Research dates: 2026-10-02; updated 2026-10-06 for Sprint 15

## Summary

For a normal Android application using the public `MediaProjection` API, PhoneCast should assume that the device must remain:

- unlocked;
- logically awake;
- in an active MediaProjection session.

PhoneCast can keep the display awake and reduce its brightness, but locking the device stops screen projection on Android 15 QPR1 and newer. A stopped projection cannot be resumed; the user must start another session and grant capture consent as required.

Samsung Link to Windows / Microsoft Phone Link provides a better experience on supported devices: it makes the physical phone screen black and apparently off while mirroring and remote interaction continue. This is not ordinary screen locking and is not described as simple brightness reduction.

The exact implementation used by Samsung and Microsoft is not publicly documented. Because Phone screen support is limited to selected devices—typically with Link to Windows preinstalled—it should be treated as OEM/system integration rather than a capability available to an ordinary Play-installed application.

## Confirmed behavior

### Public Android MediaProjection

Android documents that:

- screen capture uses a user-authorized `MediaProjection` session;
- Android 14 and newer require user consent for each new projection session;
- the system calls `MediaProjection.Callback.onStop()` when projection terminates;
- screen locking is one reason a projection terminates;
- on Android 15 QPR1 and newer, projection automatically stops when the device locks;
- a stopped projection cannot be reused to create another virtual display.

Therefore, a foreground service, network reconnection, or keep-alive mechanism cannot restore a projection that Android has invalidated.

Source:

- [Android Developers — Media projection](https://developer.android.com/media/grow/media-projection)

### Samsung Link to Windows / Microsoft Phone Link

Microsoft documents an option named:

> Hide my Android device's screen while it's connected to my PC.

While enabled, Microsoft says the Android screen turns black and appears to be off. Mirroring and PC-side interaction continue. The user can dismiss the black screen by pressing the power button, swiping the device screen, or activating Bixby. Microsoft describes the feature as protecting privacy and minimizing battery consumption.

Samsung similarly states that the phone screen is blank during screen mirroring and can be restored by swiping the screen or pressing the Side button.

This behavior is distinct from locking the phone: the remote session remains active while only the local presentation is hidden.

Sources:

- [Microsoft Support — Setting up and using Phone screen in Phone Link](https://support.microsoft.com/en-us/windows/apps/phonelink/setting-up-and-using-phone-screen-in-the-phone-link)
- [Samsung Support — Galaxy phone mirroring and Android apps with Link to Windows](https://www.samsung.com/us/support/answer/ANS10001340/)

### Device restrictions

Microsoft states that Phone screen is available on selected Android devices running Android 10 or newer with Link to Windows preinstalled. This includes supported Samsung Galaxy devices and selected devices from other manufacturers.

This device gating supports the conclusion that the complete experience depends on manufacturer integration. It does not prove which private permission or internal API is used.

Source:

- [Microsoft Support — Setting up and using Phone screen in Phone Link](https://support.microsoft.com/en-us/windows/apps/phonelink/setting-up-and-using-phone-screen-in-the-phone-link)

### scrcpy and ADB

scrcpy can turn off the device's physical screen while mirroring continues:

```text
scrcpy --turn-screen-off
```

Its documentation also records an Android 15 shell command:

```text
adb shell cmd display power-off 0
adb shell cmd display power-on 0
```

This demonstrates that panel-off mirroring is technically possible when software has ADB/shell-level access. It does not establish that a normal Android application can issue the same operation.

Source:

- [scrcpy — Device documentation](https://github.com/Genymobile/scrcpy/blob/master/doc/device.md)

## What is not publicly established

The reviewed Microsoft and Samsung documentation does not explain whether Phone Link:

- powers down the physical display panel;
- presents a system-level black privacy surface excluded from capture;
- uses Samsung-specific display APIs;
- uses privileged MediaProjection integration;
- uses another OEM-only remote-display pipeline.

It is therefore inaccurate to claim a specific mechanism without further primary evidence.

What can safely be stated is:

1. Phone Link does not merely describe lowering brightness.
2. It blanks or hides the local phone display while remote mirroring remains active.
3. The phone is not being conventionally locked during this state.
4. The feature is restricted to supported, integrated devices.
5. No equivalent public Android API has been identified for ordinary applications.

## Implications for PhoneCast

### Standard supported mode

PhoneCast should implement a conventional mode using public Android APIs:

1. The user starts casting and approves MediaProjection.
2. PhoneCast keeps the device awake while casting.
3. The physical display remains logically on and the phone remains unlocked.
4. The user may manually lower brightness, or PhoneCast may offer a carefully designed brightness-assistance option.
5. PhoneCast restores any setting it changes when casting ends.
6. If the phone locks, PhoneCast reports that capture ended and guides the user through starting a new session.

Panel brightness is normally applied after display composition, so reducing physical brightness should not darken the encoded stream. This must still be tested across supported devices.

### Brightness-control limitations

An activity can set brightness for its own window, but that setting may not remain effective after the user switches to another app. Changing global system brightness requires additional access and is intrusive. If PhoneCast modifies global brightness, it must:

- obtain explicit user approval;
- save the previous value;
- restore it on normal stop and best-effort recovery after failure;
- avoid disabling adaptive brightness without clear consent;
- provide an emergency way to restore visibility.

A black application overlay is not automatically a solution because it may also appear in the captured stream. Behavior involving secure or system overlays is device-dependent and must not be assumed.

### Optional advanced panel-off mode

A future advanced mode could investigate ADB or Shizuku-style delegated shell access to control display power while keeping the device logically awake. This mode would:

- require explicit developer/advanced setup;
- not be suitable as the default consumer experience;
- need per-version and per-manufacturer testing;
- need a reliable physical and remote recovery path;
- need testing against Android 15 QPR1 lock-triggered projection termination;
- need clear warnings about security and support limitations.

The first experiment should determine whether `cmd display power-off 0` turns off only the physical panel without causing a lock event or terminating PhoneCast's active MediaProjection session on the target Samsung device.

### OEM integration

Achieving behavior equivalent to Link to Windows without ADB may require:

- Samsung/OEM partnership;
- installation as a privileged system application;
- an approved manufacturer SDK;
- enterprise device-owner or Knox capabilities, if an applicable API exists.

No currently reviewed public Samsung or Android API establishes such a path, so this must remain research rather than a product assumption.

## Recommended product decision

For Sprint 15:

- enable **Dim phone while casting** by default, with a clear opt-out;
- require Android's explicit Modify system settings approval before applying it;
- keep the display logically awake and use a safe nonzero minimum brightness;
- restore the prior manual/adaptive behavior on every reachable exit path;
- clearly explain that locking the phone ends capture;
- detect `MediaProjection.Callback.onStop()` and present a clean restart flow;
- do not claim Samsung Phone Link-style screen-off support.

Track an optional **Advanced panel-off mode** as a separate experiment using ADB/Shizuku. Do not make it part of the normal architecture until it is physically validated and its permission, recovery, and security implications are acceptable.

## Sprint 15 research update: automatic dim and temporary wake

### Product decision

**Dim phone while casting is opt-out and enabled by default.** A fresh install,
and an existing install with no saved Sprint 15 preference, starts with the
preference enabled. The user can turn it off before or during casting, and doing
so restores any PhoneCast-owned brightness change immediately.

Default-on does not bypass Android consent. Device-wide dimming requires the
separate **Modify system settings** special access. When the preference is on but
access is absent, PhoneCast must explain the purpose in context and offer the
system approval screen. Declining must leave casting functional and undimmed,
without repeated coercive prompts. The implementation should model these states
separately:

```text
Preference: enabled / disabled
Capability: granted / unavailable
Session: original / dimmed / temporarily restored / recovery needed
```

### Public API findings

`WindowManager.LayoutParams.screenBrightness` is insufficient for PhoneCast's
normal flow. Android documents it as a per-window override that applies while
that window is in front; it cannot keep another foreground application's panel
dimmed while PhoneCast captures it.

The applicable public device-wide path is `Settings.System`:

- declare `android.permission.WRITE_SETTINGS`;
- check `Settings.System.canWrite(context)`;
- direct the user to `Settings.ACTION_MANAGE_WRITE_SETTINGS` for explicit special
  access on API 23 and newer;
- read and write `SCREEN_BRIGHTNESS` and `SCREEN_BRIGHTNESS_MODE` only after that
  approval.

Android documents `SCREEN_BRIGHTNESS` as 1–255 and separately exposes automatic
and manual modes. A predictable minimum therefore requires temporarily using
manual mode. PhoneCast should persist the original mode and brightness before
its first write, leave the automatic-brightness adjustment itself untouched,
and restore brightness before restoring the original mode. The initial dim value
must be nonzero and physically validated on the target phone; it must not assume
that every OEM maps the same numeric value to the same usable luminance.

### Restoration and failure model

Before changing a system setting, synchronously persist a recovery record with:

- original brightness;
- original automatic/manual mode;
- whether PhoneCast currently owns the override;
- a session/generation identifier so stale cleanup cannot overwrite a later user
  choice.

Restore on explicit Stop, preference opt-out, projection `onStop()`, service
teardown, permission loss, and normal disconnect policy. Attempt recovery at the
next PhoneCast process start and from a boot receiver where Android permits it.
Observe brightness-setting changes while the override is active: a deliberate
user change should cancel PhoneCast ownership or become the new baseline rather
than being silently overwritten later.

This recovery is necessarily best-effort. Android 15 keeps a force-stopped app in
the stopped state until direct or indirect user action, so PhoneCast cannot run
cleanup at the instant of Force Stop or rely on a boot callback while still
stopped. The safety design therefore also requires a nonblack dim value, an
in-app and foreground-notification **Restore brightness** action, clear guidance
that Android Quick Settings remains the emergency recovery path, and startup
recovery before another dim attempt. Documentation and tests must not claim
instant force-stop restoration.

### Physical-interaction detection

Android's accessibility API defines `TYPE_TOUCH_INTERACTION_START` and
`TYPE_TOUCH_INTERACTION_END` as system-generated events for the user starting and
ending a screen touch. PhoneCast's existing optional remote-control accessibility
service can subscribe without retrieving window content, so this is the most
credible public experiment for temporarily restoring brightness after physical
interaction.

It is not yet accepted behavior:

- the service is optional and cannot be silently enabled for a default-on dim
  preference;
- the events provide no raw coordinates or trustworthy physical-versus-injected
  source marker;
- the target device must establish whether PhoneCast's own `dispatchGesture()`
  calls also produce these events;
- using accessibility for dim/wake must be added to PhoneCast's prominent
  disclosure and Play Console declaration because the service is explicitly not
  an accessibility tool.

Do not request touch-exploration mode, install a touch-consuming full-screen
overlay, inspect UI content, or claim raw global touch input. If the accessibility
service is unavailable, the supported baseline is default-on timed dimming plus
explicit temporary-restore controls; automatic physical-touch wake must be shown
as unavailable rather than inferred.

### Implemented baseline — awaiting physical validation

The Android sender now implements the researched baseline:

1. The persisted preference defaults to enabled and can be turned off at any
   time. A 5, 15, 30, or 60 second idle delay is selectable; 15 seconds is the
   default.
2. First cast explains Modify system settings access. The user can open Android's
   approval screen, cast undimmed, or turn the feature off. A declined prompt is
   not repeated automatically; Settings retains an explicit capability button.
3. A testable controller commits the original brightness/mode and expected
   PhoneCast state before writing global settings. Dimming switches to manual
   mode and brightness value 1. Temporary and final restoration write brightness
   before restoring the original mode.
4. If brightness differs from PhoneCast's expected value at final restoration,
   the current level is preserved as a deliberate user change while the original
   automatic/manual policy is restored.
5. Recovery runs at application startup and `BOOT_COMPLETED` where Android allows
   it. Force Stop remains an unavoidable gap until the user starts/interacts with
   PhoneCast again, and revoked Modify system settings access prevents restoration
   until access returns.
6. The capture service holds a screen-dim wake lock while projection is active,
   releases it on teardown, restores brightness on reachable stop paths, and
   offers **Restore brightness** in both Settings and the foreground notification.
   Temporary restoration restarts the configured idle timer.
7. The existing optional accessibility service subscribes to the system-only
   touch-start event. It does not request touch exploration or receive coordinates.
   A bounded suppression interval surrounds PhoneCast's own `dispatchGesture()`
   calls; target-device testing must still determine whether injected and physical
   events can be distinguished reliably.

JVM tests cover default nonzero dimming, adaptive/manual restoration, repeat
callbacks, temporary restore/redim, denied capability, preservation of user
brightness changes, interrupted-process recovery, and failed restoration. Android
unit tests, debug APK assembly, and lint pass. None of these software checks proves
panel luminance, MediaProjection isolation, wake behavior, accessibility event
delivery, or recovery on the physical target phone.

The stronger requested privacy mode remains research. A real Android lock is
prohibited because Android 15 QPR1 and newer stop MediaProjection and invalidate
the session. A capture-visible black overlay is also not useful. No supported
public API reviewed here provides Phone Link-style local-only blanking for a
normal application.

## Suggested validation matrix

Test at least:

| Case | Expected result |
|---|---|
| Default preference with special access granted | Phone dims after the configured delay |
| Default preference with access declined | Casting continues undimmed without repeated prompts |
| Manual brightness before casting | Exact prior brightness and manual mode return on stop |
| Adaptive brightness before casting | Adaptive mode and prior brightness behavior return on stop |
| Minimum nonzero panel brightness | Remote stream remains normally visible and the local UI remains recoverable |
| Physical touch with accessibility service enabled | Temporary wake behavior is measured without consuming the touch |
| VR-injected gesture | Determine whether it emits the same accessibility touch event |
| Accessibility service disabled | Explicit restore works; automatic touch wake is not claimed |
| Projection revoke, permission removal, process death, reboot | Each reachable path restores or records recovery independently |
| Force Stop | Limitation is visible; subsequent user launch restores before another dim |
| User changes brightness while dimmed | PhoneCast does not later overwrite the deliberate choice |
| User presses power/locks phone | Projection stops on Android 15 QPR1+ and restart UI appears |
| Portrait/landscape transition while dimmed | Stream, brightness ownership, and encoder reconfiguration remain functional |
| Protected/`FLAG_SECURE` application | Protected content remains excluded as required |
| ADB/Shizuku/panel-off integration | Explicitly rejected and out of scope |

## Sprint 15 closure — 2026-10-06

The project owner physically used the implemented default-on dimming flow and
approved Sprint 15. The owner reports that ordinary dimming, temporary
restoration, and casting behavior are working as intended. This approval closes
the supported public-API brightness baseline; it does not claim every row of the
expanded failure matrix was separately witnessed or that minimum brightness is a
measured panel-power benchmark.

The proposed advanced Developer Options / ADB / Shizuku panel-off approach was
reviewed after implementation and **explicitly rejected by the project owner**.
Do not add, expose, document as a product option, or treat as a pending Sprint 15
requirement any ADB, Shizuku, shell, display-power, or panel-off integration.
PhoneCast remains limited to its approved public-API, awake-and-unlocked,
minimum-brightness mode. Android locking continues to terminate MediaProjection
as documented.

Known platform constraint retained at closure: Force Stop cannot run immediate
brightness cleanup. PhoneCast records recovery before changing brightness and
attempts restoration on a later app start or boot when it retains Modify system
settings access. Android Quick Settings remains the recovery route if that
best-effort path cannot run.

## Primary references for the Sprint 15 update

- [Android `Settings.System`](https://developer.android.com/reference/android/provider/Settings.System) — `canWrite`, `SCREEN_BRIGHTNESS`, and brightness mode.
- [Android `Settings.ACTION_MANAGE_WRITE_SETTINGS`](https://developer.android.com/reference/android/provider/Settings#ACTION_MANAGE_WRITE_SETTINGS) — user-managed special access.
- [Android `WindowManager.LayoutParams.screenBrightness`](https://developer.android.com/reference/android/view/WindowManager.LayoutParams#screenBrightness) — foreground-window-only override.
- [Android `AccessibilityEvent.TYPE_TOUCH_INTERACTION_START`](https://developer.android.com/reference/android/view/accessibility/AccessibilityEvent#TYPE_TOUCH_INTERACTION_START) — system touch-interaction signal.
- [Google Play AccessibilityService policy](https://support.google.com/googleplay/android-developer/answer/10964491) — declaration, prominent disclosure, and consent requirements for non-accessibility tools.
- [Android 15 behavior changes](https://developer.android.com/about/versions/15/behavior-changes-all) — force-stop/stopped-state behavior.
- [Android media projection](https://developer.android.com/media/grow/media-projection) — lock-screen termination and invalid projection lifecycle.

## Samsung background timeout follow-up — 2026-10-07

The owner's SM-S906U (S22+) running Android 16 locks when casting from other apps. Read-only power logs show Samsung classifying the casting screen-dim wake lock as `abuse wakelock`, disabling its contribution and sleeping due to timeout. Foreground-only keep-screen-on is not sufficient for phone mirroring.

The sender now temporarily writes `Settings.System.SCREEN_OFF_TIMEOUT` to `Integer.MAX_VALUE` (2,147,483,647 ms, approximately 24.85 days) while projection is active, with the existing explicit Modify system settings capability. This is independent of the dimming preference. The original timeout is synchronously saved before writing and restored on teardown; app-start/boot recovery handles abandoned ownership best-effort. User timeout changes are preserved. Permission revocation, write failure, and Force Stop can delay restoration; Android Display settings remain the emergency recovery path. Without access, casting remains available with an automatic-lock warning. Grant access before starting a new cast.

Manual locking remains untouched and ends projection normally. OEM clamping and device-admin maximum-lock policy remain possible; readback verifies the setting, not the effective awake policy. JVM tests, APK assembly, and lint pass. No device setting was changed or APK installed during the initial implementation; the subsequent authorized installation and physical validation are recorded below.

The timeout-fix APK was release-signed and published to the local download server on 2026-10-07 as v0.8 `versionCode` 4. Debug/release JVM tests, both APK builds, and debug/release lint pass. `apksigner` verified the same certificate as the original code-3 release, permitting an in-place release update. Both live APK routes match the release SHA-256 `963be8172accc7539ff768f870932b44493ae98716fceb6d2edaf1a796580227`; served `SHA256SUMS` was updated without changing the receiver archive. This is local publication only, not a GitHub release update or physical phone validation. Stop casting before installing, retain Modify system settings approval, and start a new cast afterward.

### Physical confirmation — 2026-10-07

The initial reported 15-second lock occurred with the original release (`versionCode` 3) still installed; it was not a test of the fix. With owner authorization, the signed code-4 APK was installed using `adb install -r`, and package inspection confirmed code 4. The owner then reported that idle casting worked without the prior 15-second lock.

Read-only `PowerManagerService` logs corroborate both application and restoration:

- 21:32:06: configured timeout was 15,000 ms.
- 21:32:19: system setting and effective screen-off timeout changed to 2,147,483,647 ms (about 24.85 days).
- 21:33:35: timeout returned to 15,000 ms as the cast ended; capture-stop logs followed at 21:33:36.
- 21:33:52: normal sleep occurred due to the restored 15-second timeout.

Samsung Settings showed **10 minutes**, its maximum selectable preset, while logs reported the actual much larger timeout. The displayed preset is not the authoritative value for this temporary override. Do not manually select another timeout during casting unless intentionally replacing the override; user changes are preserved.

This confirms the owner's Samsung S22+ / Android 16 idle-casting and normal restoration case, not every device or failure path. Dimming-disabled testing, manual-lock testing of this build, permission-loss, and interrupted-session recovery remain separate checks. Raw logs are ignored under `out/diagnostics/phone-timeout-followup/`.

The initial distribution recommendation was a separate **v0.8.1** patch with the same signing key, release notes and fresh checksums rather than silently replacing published v0.8 artifacts. The owner subsequently chose **v0.9.0** for the connection/About/export/discovery work; that code-5 test candidate includes this timeout fix and is tracked in `docs/v0.9-support.md`. The timeout fix alone does not require a receiver reinstall; v0.9 discovery/About features do require the new receiver. No GitHub release was created or modified by this validation.

The owner additionally requested future optional developer/ADB research in `design.md`, superseding the prior prohibition on researching that option. No privileged integration is implemented. scrcpy's [device controls](https://github.com/Genymobile/scrcpy/blob/master/doc/device.md) distinguish privileged periodic user activity, charging-only stay-awake, temporary timeout changes, and panel-power control. These are not equivalent to a normal app wake lock.

## Conclusion

Samsung Phone Link does not appear to solve this by merely dimming the phone. It offers a special black/hidden local-display state while remote mirroring continues. Public documentation confirms the behavior but does not disclose its implementation. PhoneCast will therefore use default-on, user-disableable minimum-brightness operation as the supported Sprint 15 baseline, subject to explicit Android special-access approval and best-effort recovery. Panel-off operation remains limited to future privileged or advanced experiments.
