# RDP-to-console VRLink recovery investigation

Last reviewed: 2026-10-03

Status: **Paused — recovery automation works, sustained VRLink streaming does not**

## Scope

This investigation concerns PC SteamVR/VRLink streaming to Steam Frame after the
Windows desktop has been accessed through Microsoft Remote Desktop. It is a host
runtime and test-environment issue, not currently evidence of a PhoneCast video,
decoder, or overlay defect.

Do not mark RDP recovery complete until an ordinary SteamVR stream remains stable
through the acceptance test below.

## What is implemented

`scripts/windows/Start PhoneCast VR.cmd` detects an RDP session and currently:

1. stops PhoneCast, SteamVR, and Steam;
2. starts a detached continuation helper;
3. runs elevated `tscon <session-id> /dest:console`;
4. waits until `query session` reports the same user session as `console`;
5. waits ten seconds for the physical display stack to settle;
6. starts Steam, SteamVR, and PhoneCast in that console session;
7. checks process session ownership and writes recovery logs under `out/logs`.

The transfer and console-side launches have been physically observed. This does
not make the resulting VRLink stream stable.

Two diagnostic launchers are available:

- `scripts/windows/Test VR - No PhoneCast.cmd` — SteamVR baseline with PhoneCast stopped.
- `scripts/windows/Test VR - Visible PhoneCast.cmd` — visible, centered, head-locked PhoneCast
  waiting panel; saved settings are not read or written and controller input is
  not required to reveal it.

Both record one-second Windows session/process samples for ten minutes in
`out/logs/session-<mode>-<timestamp>.jsonl`.

## Confirmed findings

### PhoneCast is not required to reproduce the freeze

The SteamVR-only baseline froze in under one minute, recovered temporarily after
a headset disconnect/reconnect, then returned to the waiting/loading view.
PhoneCast was not running. The VRLink stability fault must therefore be resolved
before using headset visibility as evidence about PhoneCast.

### The RDP client automatically reconnects

The user did not manually reconnect during the baseline, but Windows Terminal
Services recorded remote reconnections from `10.0.0.176`. Relevant events:

- 14:33:47 — RDP reconnection;
- 14:35:08 — RDP reconnection;
- 14:35:33–14:35:34 — recovery transferred session 4 back to `console`;
- 14:38:06–14:38:07 — RDP automatically reattached session 4 to `rdp-tcp#0`.

The session monitor independently recorded the 14:38 transition. Automatic RDP
reconnection is therefore real and can eventually invalidate an otherwise good
console session.

It does not explain the first freeze in this run: compositor timeouts began near
14:36:01, while session 4 remained attached to `console` until 14:38:06.

RDP automatic reconnect is controlled by the client. Before the next test, on the
client machine uncheck **Experience → Reconnect if the connection is dropped** or
set this in the saved `.rdp` file:

```text
autoreconnection enabled:i:0
```

Close any retry dialog after recovery disconnects RDP. PhoneCast has not changed
the RDP client's setting.

Microsoft reference:
[Supported RDP properties](https://learn.microsoft.com/en-us/azure/virtual-desktop/rdp-properties)

### The dedicated Steam Frame wireless link is failing to associate

The strongest early-freeze evidence is in Steam's `remote_connections.txt`:

```text
14:35:49  Frame accepted over ordinary LAN at 10.0.0.174
14:35:50  Dedicated adapter reinitialized; WlanConnect_ to frame_4E95B351_1
14:35:54  Adapter reinitialized again
14:35:55  WlanConnect_ ..._2
14:36:00  WlanConnect_ ..._3
14:36:01  SteamVR compositor WaitForAcquire timeout storm begins
14:36:04  WlanConnect_ ..._4
14:36:12  Connect timeout event: frame_4E95B351
```

The host's dedicated adapter is:

```text
Realtek 8832CU Wireless LAN WiFi 6 USB NIC For Valve
Driver 5.32.908.2026
5 GHz and 6 GHz supported
```

After the failed run, `netsh wlan show interfaces` reported `Wi-Fi 2` as
`associating`. The logs show a good ordinary-LAN connection followed by repeated
attempts to move/add the dedicated `frame_*` link. This supports a failed adapter
or Multi-Link handoff as the leading cause of the 10–60-second failure.

The sequence also explains why disconnecting/reconnecting the headset can work
briefly: VRLink can reconnect through ordinary LAN before another dedicated-link
attempt fails.

Supporting references:

- [Steam Frame wireless-adapter Windows report](https://steamcommunity.com/app/4165890/discussions/1/590690402783479749/)
- [Steam network and Wi-Fi troubleshooting](https://help.steampowered.com/en/faqs/view/3548-a9f4-02a3-940f)

### VRLink, not the application, stops acquiring compositor textures

`vrcompositor.txt` repeatedly reports:

```text
WaitForAcquire timed out (FAILED); rendering the next frame before the driver took the sync texture
```

VRLink also emits stream-reset requests and `Connection inactive`. This indicates
the wireless streaming driver is no longer consuming compositor output. It does
not identify PhoneCast texture submission as the cause, especially because the
SteamVR-only baseline reproduces it.

## Relevant host state

At the last inspection:

- Windows user session: ID 4;
- SteamVR: 2.17.10, build ID 25330290;
- Steam client: stable channel (`BetaCandidate` empty);
- GPU: NVIDIA GeForce RTX 3060, driver 32.0.16.1088;
- secondary GPU: AMD Radeon Graphics;
- physical host network: Realtek 2.5 GbE, linked at 1 Gbps;
- Frame ordinary-LAN address observed: `10.0.0.174`;
- RDP client address observed: `10.0.0.176`;
- dedicated Frame adapter: Realtek 8832CU USB;
- additional active virtual adapters: VMware VMnet1/VMnet8 and Hyper-V/WSL
  switches.

The ordinary LAN route was selected correctly before the dedicated-link retries,
so virtual adapters remain a secondary hypothesis rather than the leading one.

## Next test sequence

Perform one variable change at a time and retain exact timestamps.

### Test 1 — clean physical-console control

Purpose: establish whether any residual RDP state remains relevant.

1. Disable automatic reconnect on the RDP client.
2. Reboot the PC and Steam Frame.
3. Log in physically; do not establish RDP at any point after reboot.
4. Start Steam and SteamVR normally without PhoneCast.
5. Stream for at least ten minutes.
6. Record whether the waiting/loading failure occurs and its exact time.

If this fails, RDP is not required for the current fault.

### Test 2 — ordinary LAN only

Purpose: isolate the dedicated Frame adapter and Multi-Link transition.

1. Keep RDP automatic reconnect disabled.
2. Disconnect or disable the dedicated Realtek 8832CU Frame adapter before
   starting Steam/SteamVR.
3. Confirm the Frame and PC are connected through the same ordinary LAN.
4. Run `scripts/windows/Test VR - No PhoneCast.cmd` from a deliberately established RDP session,
   or start SteamVR physically for the cleanest control.
5. Stream for at least ten minutes.

Expected diagnostic result:

- stable LAN-only streaming strongly implicates the dedicated adapter/Multi-Link;
- the same early freeze shifts priority toward VRLink/SteamVR version, GPU driver,
  or persistent RDP graphics state.

### Test 3 — Multi-Link off, dedicated adapter present

If the UI exposes **Allow multiple links** or **Multi-Link**, disable it and choose
one transport deliberately. Test ordinary Wi-Fi/LAN first, then adapter-only if
available. Do not combine this with other network changes.

### Test 4 — dedicated adapter remediation

Only if Tests 2–3 implicate the adapter:

1. use a direct motherboard USB 3.x port, not a hub or front-panel extension;
2. inspect/disable USB and adapter power saving;
3. reboot and re-pair the Frame adapter;
4. verify the adapter reaches `connected`, not indefinitely `associating`;
5. verify `remote_connections.txt` no longer repeats `WlanConnect_` followed by
   `Connect timeout event`.

### Test 5 — virtual adapter isolation

If LAN-only still fails, temporarily disable VMware and unused Hyper-V/WSL virtual
adapters, leaving physical Ethernet enabled. Reboot Steam/SteamVR and retest. Do
not remove virtualization configuration; this is a reversible isolation test.

### Test 6 — software-version comparison

If network isolation is inconclusive:

1. verify SteamVR files;
2. record Steam, SteamVR, Frame OS/firmware, and GPU driver versions;
3. compare SteamVR stable with an available beta or previous branch, one branch
   change at a time;
4. save a SteamVR System Report after a failure.

Do not treat community reports about another headset/runtime as proof; branch
changes are diagnostic experiments.

## Evidence to collect after each run

Record:

- test name and exact start/freeze/reconnect times;
- whether RDP had ever connected since the last reboot;
- whether RDP automatic reconnect was disabled;
- dedicated adapter connected/disconnected and Multi-Link state;
- whether audio/input continued after video froze;
- whether the desktop SteamVR mirror continued rendering;
- whether headset disconnect/reconnect recovered temporarily.

Preserve:

```text
out/logs/session-*.jsonl
C:\Program Files (x86)\Steam\logs\remote_connections.txt
C:\Program Files (x86)\Steam\logs\driver_vrlink*.txt
C:\Program Files (x86)\Steam\logs\vrcompositor*.txt
C:\Program Files (x86)\Steam\logs\vrserver*.txt
```

Useful read-only checks:

```powershell
query session
netsh wlan show interfaces
Get-NetAdapter -IncludeHidden
```

Steam rotates logs on restart. Copy them immediately after a failure before the
next recovery attempt.

## Acceptance criteria

RDP recovery is successful only when all of the following are physically proven:

1. the recovered user session remains attached to `console`;
2. no automatic RDP reconnection occurs;
3. an ordinary SteamVR stream remains responsive for at least ten minutes;
4. no sustained `WaitForAcquire` storm or dedicated-link timeout occurs;
5. headset disconnect/reconnect is not required to maintain the stream.

After that baseline passes, run `scripts/windows/Test VR - Visible PhoneCast.cmd`. Confirm the
waiting panel before starting Android casting, then validate phone video and
controller input separately.
