# Android Screen-Off Behavior During Phone Casting

## Purpose

This document records whether PhoneCast can continue capturing an Android phone while its physical display is off, and how Samsung Link to Windows / Microsoft Phone Link handles this experience.

Research date: 2026-10-02

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

For the initial PhoneCast release:

- support **Keep screen awake while casting**;
- provide guidance to use minimum brightness;
- investigate safe brightness assistance separately;
- clearly explain that locking the phone ends capture;
- detect `MediaProjection.Callback.onStop()` and present a clean restart flow;
- do not claim Samsung Phone Link-style screen-off support.

Track an optional **Advanced panel-off mode** as a separate experiment using ADB/Shizuku. Do not make it part of the normal architecture until it is physically validated and its permission, recovery, and security implications are acceptable.

## Suggested validation matrix

Test at least:

| Case | Expected result |
|---|---|
| Minimum panel brightness | Remote stream remains normally visible |
| `FLAG_KEEP_SCREEN_ON` during capture | Screen does not time out while PhoneCast activity is visible |
| User switches to another app | Keep-awake and brightness behavior is recorded |
| User presses power/locks phone | Projection stops on Android 15 QPR1+ and restart UI appears |
| ADB `display power-off` during projection | Determine whether stream continues without a lock event |
| Restore display through ADB and physical power button | Device always remains recoverable |
| Orientation change while panel is off | Stream and encoder reconfiguration remain functional |
| Protected/`FLAG_SECURE` application | Protected content remains excluded as required |

## Conclusion

Samsung Phone Link does not appear to solve this by merely dimming the phone. It offers a special black/hidden local-display state while remote mirroring continues. Public documentation confirms the behavior but does not disclose its implementation. PhoneCast must therefore treat minimum-brightness, awake-and-unlocked operation as the supported baseline, with panel-off operation limited to future privileged or advanced experiments.
