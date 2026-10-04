# Notification bridge

Last reviewed: 2026-10-03

Sprint: 9 — complete for the PC-hosted path; accepted by the project owner on 2026-10-04

## User and privacy model

Notification forwarding is optional and disabled by default. It has two independent requirements:

1. **Forward notifications to VR while casting** must be enabled in PhoneCast Sender.
2. The user must explicitly grant PhoneCast Android notification access.

By default, forwarded cards contain only the source application label and the generic text **New notification**. Notification titles and body text are sent only after the user explicitly enables **Include notification titles and message text (sensitive)**.

The sender supports comma-separated Android package-name filters:

- a non-empty allowlist forwards only matching packages;
- the blocklist always wins and excludes matching packages;
- a blank allowlist permits all packages not blocked;
- PhoneCast's own foreground-service notification is always excluded;
- notification group summaries are excluded to avoid duplicate cards.

The filters and privacy choices are stored in the sender's private preferences. Notification content, titles, and package names are never written to PhoneCast logs. Logs record only that a privacy-filtered event was forwarded or displayed.

Android notification access is broad OS-level permission. The service reads only posted notification metadata needed to construct an allowed card. It does not forward notifications unless casting is active, because the bridge sends only through the active phone-initiated receiver connection.

## Data path

```text
Android NotificationListenerService
    -> disabled-by-default privacy and package filters
    -> bounded NOTIFICATION protocol message
    -> authenticated active TCP streaming session
    -> portable NotificationEvent
    -> OpenVR transient notification overlay
```

The notification payload contains a version, redaction flag, bounded UTF-8 application/title/body/package fields, and Android post time. Each text field is limited to 1024 bytes and the common protocol retains its 4 MiB allocation limit. The receiver validates every length before allocation or display.

Notification traffic has its own bounded eight-card Android queue. Receiver queue handling keeps cards outside H.264 sequence and keyframe recovery state, so a notification cannot invalidate the video prediction chain. Under receiver congestion, an older card is discarded rather than a video access unit.

## VR behavior

The Windows OpenVR backend displays the newest card for six seconds in a small head-relative panel near the upper-right of central view. A newer card replaces the current card. The card is independent of full-phone Hidden/Glance/Expanded/Pinned state. The card uses a rounded, layered surface with an application-initial badge, accent hierarchy, privacy-aware body text, and a distinct **Open phone** action instead of an undifferentiated text block. Actual Android application icon pixels are not transported; the earlier `PC` badge was a PhoneCast placeholder, not a working app icon.

Open **PhoneCast → Settings → Notifications** in the SteamVR dashboard to adjust card **Size**, **Distance**, **Horizontal**, and **Vertical** placement. Entering the category displays a representative styled card at the current placement; sliders and `-` / `+` changes move and resize it immediately. Leaving the category or closing settings removes the preview. Changes are reverted by Cancel and persist only after Apply. The safer revised defaults place the card closer to center and nearer to the user than the original Sprint 9 prototype.

When the SteamVR dashboard laser is available, selecting the card dismisses it, opens the full phone in Expanded state, and asks Android to invoke that notification's original `contentIntent`, matching a tap on the phone when the source app supplied a valid action. Android retains only a bounded, session-scoped map from opaque tokens to `PendingIntent` objects; no Intent contents cross the network. On Android 14+, PhoneCast explicitly marks this user-initiated `PendingIntent.send` as an allowed background activity start; without that sender opt-in, Android can accept the pending intent without bringing Messenger or another target activity to the foreground. See Android's [target-SDK 34 background activity launch changes](https://developer.android.com/about/versions/14/behavior-changes-14#background-activity-starts). Removed, expired, actionless, or cancelled notifications still open the phone view but cannot navigate to a target. The notification overlay deliberately does not enable OpenVR's global-interactive flag, because that flag previously stole controller input from running games. The card is therefore informational while the dashboard is closed.

The current portable renderer contract is reusable by the future Steam Frame ARM64 backend. Native display behavior is not validated by this sprint.

## Security limitations

Notification data uses the existing paired TCP stream. That transport is not encrypted and the six-digit code is not cryptographic authentication. Notification forwarding, especially content forwarding, must be used only on a trusted LAN until authenticated encryption and durable device identity are implemented.

No replies, notification dismissal, notification history, icons, or images are forwarded. The only card action is opening the full phone and invoking the source notification's existing Android launch action when available.

## Validation status

Automated validation completed:

- C++ notification serialization, parsing, bounds rejection, and protocol type tests;
- Android notification payload, allowlist/blocklist, and text-cleaning JVM tests;
- Windows x64 clean incremental build and all CTest tests;
- Android JVM tests, debug APK assembly, and lint;
- Docker Linux ARM64 cross-build produced an AArch64 receiver executable (compilation only; no native runtime validation).

Physical validation and closure result:

- the post-Sprint feedback pass physically confirmed notification preview/placement and notification selection opening the source Android application (including Messenger), and the project owner approved the round on 2026-10-04;
- the card is visible with the SteamVR dashboard both open and closed;
- dashboard-laser selection opens the full phone;
- the original fixed placement was reported as too far into the upper-right and difficult to see;
- the revised default placement was physically approved;
- the project owner closed Sprint 9 for the PC-hosted path on 2026-10-04.

Deferred evidence that must not be represented as physically tested:

- every allowlist/blocklist combination;
- redacted versus sensitive-content card variants;
- notification-access revocation and immediate-disable behavior;
- running-game controller-input coexistence specifically while a card is visible;
- reconnect and rotation behavior during notification delivery;
- native ARM64 behavior, which remains Sprint 11 work.

Actual Android application icon pixels are not implemented or physically validated; cards intentionally show the source application's initial.

These paths have implementation and automated coverage where applicable, but Sprint 9 closure is based on owner acceptance of the core physical notification experience rather than a claim that every privacy/lifecycle permutation was manually exercised.
