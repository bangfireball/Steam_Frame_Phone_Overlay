# PhoneCast VR

**Your Android phone, inside Steam Frame. No PC required.**

Mirror your phone into VR, check notifications, watch videos with optional phone audio, and interact using your controllers—all while a standalone game runs underneath.

**v0.8 · First public release preparation · Early-access software**

[Downloads & release notes](https://github.com/bangfireball/Steam_Frame_Phone_Overlay/releases) · [Report a bug](https://github.com/bangfireball/Steam_Frame_Phone_Overlay/issues) · [Build from source](docs/development.md)

## See it in action

| Phone overlay during gameplay | Dashboard and settings |
| --- | --- |
| *Gameplay photo coming soon* | *Controls photo coming soon* |

**Demo video:** coming soon—showing launch, pairing, phone interaction, and standalone gameplay.

<!-- Release media slots: replace the placeholder cells above with Markdown images.
Suggested files: docs/media/gameplay.jpg and docs/media/dashboard.jpg.
For a video, add a linked thumbnail: [![Watch the demo](docs/media/demo.jpg)](VIDEO_URL).
Use your own captures and hide pairing codes, notification text, and personal details.
-->

## What you can do

- **View your phone in VR:** local-network H.264 streaming targeting 30 FPS, with portrait and landscape support.
- **Place it where you want:** world/head placement or left/right controller docking, with move and resize handles.
- **Control Android:** tap, hold, swipe, drag-scroll, and Android Back with the dashboard laser when optional remote control is enabled.
- **Hear phone audio:** optional supported playback capture, headset volume/mute controls, and local-phone mute.
- **See notifications:** optional cards, app allow/block lists, and notification-to-app navigation; sensitive content is redacted by default.
- **Keep setup convenient:** saved receiver details, Android Quick connect widget, in-headset settings, and reconnect during an active capture session.
- **Dim the physical phone:** default-on dimming while casting, subject to Android approval, with temporary restore controls.

The primary release target is **the native Steam Frame ARM64 receiver + Android app**. A Windows SteamVR receiver also works, but is a secondary development/fallback path—not the focus of v0.8. A Windows-hosted overlay cannot follow you into headset-native standalone games.

## Before you install

> **Trusted private LAN only.** The current pairing gate does **not** encrypt video, audio, notifications, or remote-control traffic. Do not expose the receiver to the Internet or use it on an untrusted/shared network.

You need:

- **Steam Frame** with native SteamVR running.
- **Android 10 or newer**, with screen-capture and H.264 encoder support. Individual apps may block capture.
- Both devices on the same trusted local network, with phone-to-headset TCP access on port `49321`.

PhoneCast has been physically used on Steam Frame with standalone Cubism, phone interaction, notifications, rotation, and sustained phone video/audio playback. It is still early-access software: broader game compatibility, both launch orders, sleep/wake, and failure recovery are not fully validated.

## Quick start

Download the **Steam Frame ARM64 receiver archive** and **Android APK** from [GitHub Releases](https://github.com/bangfireball/Steam_Frame_Phone_Overlay/releases). Use the matching v0.8 assets, not GitHub's automatically generated source-code archives. If v0.8 is not listed yet, publication is still pending.

### 1. Install on Steam Frame

1. Download and extract the receiver `.tar.gz` archive on the headset.
2. Open a terminal in the extracted directory containing `phonecast-vr-stream-receiver` and `steam-frame-installer`.
3. Run the installer as your normal user—**not with `sudo`**:

   ```bash
   ./steam-frame-installer/install-phonecast.sh
   ```

4. With SteamVR running, open its dashboard, select **+**, and launch **PhoneCast VR**.

Installation currently needs a terminal; ordinary launches afterward do not. The installer creates a desktop entry and installs under `~/.local/opt/phonecast-vr`. A second launch is designed to focus the existing receiver instead of starting another copy. Autostart is not required or currently provided.

### 2. Install and connect Android

1. Install the downloaded APK. Android may ask you to allow installation from the browser/file manager you used.
2. Open PhoneCast and enter the **headset's local IP address** and the **six-digit pairing code shown in its PhoneCast dashboard**. Check the headset's network settings for its address; automatic discovery is not implemented.
3. Start with the **Standard** streaming profile.
4. Press **Start casting** and approve Android's screen-sharing prompt.
5. In the headset's PhoneCast dashboard, choose **Show**.

PhoneCast remembers connection details. Android may require fresh screen-sharing consent for each new capture session. If connecting fails, check the address, pairing code, and whether guest Wi-Fi/client isolation prevents devices from talking to each other.

### 3. Use it in VR

- Open the normal SteamVR dashboard and select **PhoneCast** for Show/Hide, placement, Settings, and Quit.
- **Show** places the phone at your configured world-space Start Location while preserving its size.
- With the dashboard open, point the controller laser at the phone to interact. Hold the bottom **Move** handle to reposition; use joystick push/pull while holding it to adjust depth. Use the **Resize** corner to change size.
- The phone footer keeps **Android Back**, **Move**, **Close**, and **Resize** separate. Close hides the panel; dashboard Quit stops the receiver.
- With the dashboard closed, the phone remains visible but is **view-only**, preserving game input.
- Dashboard-closed shortcuts: long-press either stick to toggle visibility; double-click a stick to dock to that hand; repeat while docked to pin in world space. These may conflict with game controls—remap or unbind them in SteamVR. Per-shortcut toggles are not implemented yet.

## Optional Android features and permissions

Casting works without enabling remote control, notifications, or phone audio.

| Feature | What to enable | Important detail |
| --- | --- | --- |
| Remote control | **Allow remote control while casting** and the **PhoneCast remote control** Accessibility service | Both gates are required. The service does not retrieve window content. Sideloaded apps may need Android's **Allow restricted settings** approval before Accessibility can be enabled. |
| Notifications | Notification forwarding and Android notification access | Disabled by default; revealing sensitive title/body text requires a separate opt-in. |
| Phone audio | **Phone playback audio** and Android audio permission; start a new cast | Playback only, not microphone capture. Protected/capture-blocked audio is unsupported. Optional local-phone mute is separate. |
| Phone dimming | **Dim phone while casting** and Android **Modify system settings** access | Preference is on by default, but without permission casting continues undimmed. Restore via the app/foreground notification; Android brightness controls are the emergency fallback. |

See [remote control](docs/remote-control.md), [notifications](docs/notifications.md), [phone audio](docs/phone-audio.md), and [phone dimming](docs/android-screen-off.md) for details.

## Known limitations in v0.8

- **Native GPU/decoder reliability is still follow-up work.** Video freezes and Adreno/Vulkan device-loss exits have occurred in some sessions, including game-active launches. Decoder recovery is implemented but its fault cases still need physical validation; bounded Vulkan recovery is not implemented. GPU resets can also affect the running game. Save game progress before testing.
- **Headset sleep/wake, SteamVR restart, crash recovery, and broader launch-order/game compatibility are not fully validated.** Relaunching PhoneCast or starting a new cast may be necessary.
- Locking the phone can end Android screen sharing. Unlock, start a new cast, and grant consent again; PhoneCast does not bypass lock-screen privacy.
- Controller-docked panels can jitter during movement. Defaults may need per-hand calibration.
- Keyboard/text entry and wrist gestures are backlogged. Hotkeys are not guaranteed conflict-free.
- Audio capture depends on the source app's policy; source UI mute is not always reflected in Android playback capture.
- Android Force Stop cannot immediately restore brightness or phone volume. Best-effort later recovery depends on retained permissions; manual recovery may be needed.
- SteamOS/SteamVR updates may change compatibility. Broader Android-device testing is welcome.

The detailed [roadmap and physical evidence](design.md) distinguish owner-approved behavior from automated tests and deferred cases. No measured photon-level latency or universal compatibility claim is made.

## Update, uninstall, and troubleshoot

**Update:** quit PhoneCast, extract the new receiver bundle, and rerun its installer. Install the matching Android APK. Use APKs signed with the same signing key for in-place Android updates.

**Uninstall receiver:** quit PhoneCast, then run:

```bash
~/.local/opt/phonecast-vr/uninstall-phonecast.sh
```

Receiver settings and pairing remain in `~/.config/phonecast-vr` unless you remove them separately.

Receiver logs normally live in `~/.local/state/phonecast-vr/receiver.log`, with diagnostics in the adjacent `performance.csv`. When [reporting an issue](https://github.com/bangfireball/Steam_Frame_Phone_Overlay/issues), include app/release version, Android device/OS, SteamOS/SteamVR version, game, launch order, and reproduction steps. Review logs before sharing; never include pairing codes, credentials, or private phone content.

## Windows receiver and development

The Windows x64 receiver is available from source for PC-hosted SteamVR use. It is secondary to the native headset release. **Never start or restart SteamVR through Windows Remote Desktop**; follow the physical-console guidance in [development instructions](docs/development.md).

Build instructions and tests:

- [Complete development setup](docs/development.md)
- [Android app](android/sender/README.md)
- [Steam Frame backend](platform/steam-frame-arm64/README.md)
- [Architecture](docs/architecture.md) · [Protocol/security](docs/protocol.md) · [Performance evidence](docs/performance.md)

## Contributing

Bug reports, device compatibility reports, documentation improvements, and focused pull requests are welcome. Preserve portable Core/VR boundaries and isolate platform-specific code. Hardware claims should name the tested device/runtime and whether the result was physically observed.

Do not commit credentials, `.env` files, device-specific secrets, Android `local.properties`, signing keys, build output, or authentication configuration.

## AI-assisted development

PhoneCast VR was developed with substantial AI coding-agent assistance for research, planning, implementation, tests, debugging, and documentation. The project owner directs the work and performs physical Android/headset validation. AI assistance is not a correctness or security guarantee; review the source, permissions, and known limitations before relying on it.

## Support development

Enjoy PhoneCast? [Buy me a coffee](https://buymeacoffee.com/pacwar) to optionally support development. PhoneCast remains free and open source; donations do not purchase features or guaranteed support.

## License

[MIT](LICENSE). Free and open source.
