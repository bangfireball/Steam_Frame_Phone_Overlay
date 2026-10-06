# Local PhoneCast download server

Optional, dependency-free Node.js server for downloading PhoneCast artifacts over the trusted local network. It is not part of the runtime architecture.

From the repository root:

```powershell
node local-apk-server/server.js
```

It listens on port `8080` by default and offers whichever artifacts currently exist:

- Android APK: `android/sender/app/build/outputs/apk/debug/app-debug.apk`
- Steam Frame ARM64 Sprint 14 bundle: `out/packages/phonecast-steam-frame-arm64-sprint14.tar.gz`

The APK path serves the current compatible sender, including the Android home-screen Quick connect widget. The stable
`/phonecast-steam-frame-arm64.tar.gz` alias and curl bootstrap select the Sprint 14
archive. This receiver bundle adds the landscape quick dashboard, reliable
Show-beside-dashboard behavior, refreshed settings, and phone resize corner while
retaining the owner-approved Sprint 13 audio path. If the current Android APK is
already installed, update only the Frame bundle and quit PhoneCast first. Windows
and ARM64 builds/tests pass; the refreshed UI still needs physical headset visual,
resize, and laser-target approval.

Override the port if needed:

```powershell
$env:PORT=8081
node local-apk-server/server.js
```

## Package the Steam Frame bundle

First complete the Linux ARM64 build and install staging described in `docs/development.md`. Then package the native receiver and adjacent OpenVR assets:

```bash
mkdir -p out/packages
tar -czf out/packages/phonecast-steam-frame-arm64-sprint14.tar.gz \
  -C out/packages phonecast-steam-frame-arm64-sprint14
```

The archive is a native AArch64 Linux application, not an APK. The download
page provides copyable command blocks for both manual extraction and curl-based
download/install. Open the page using the PC's LAN address so the curl block
uses an address reachable from Steam Frame, not `localhost`.

For an archive already downloaded to Steam Frame:

```bash
set -eu
cd "$HOME/Downloads"
tar -xzf phonecast-steam-frame-arm64-sprint14.tar.gz
sh phonecast-steam-frame-arm64-sprint14/steam-frame-installer/install-phonecast.sh
```

Or paste this block into a Steam Frame desktop terminal, replacing
`YOUR_PC_LAN_IP` with the download server's LAN address:

```bash
curl -fSLo "$HOME/install-phonecast.sh" 'http://YOUR_PC_LAN_IP:8080/install-phonecast.sh' &&
sh "$HOME/install-phonecast.sh" 'http://YOUR_PC_LAN_IP:8080'
```

Run as the normal headset user, without `sudo`. Quit PhoneCast before updating.
The `/install-phonecast.sh` bootstrap checks Linux/ARM64, refuses root, downloads
the archive before executing its installer, stops on failure, and removes the
temporary bundle files. It does not pipe remote scripts into a shell. The
bootstrap remains at `~/install-phonecast.sh` for inspection or removal; it can
also be inspected using the page's script link. Settings and pairing survive reinstalls. HTTP provides no authenticated
artifact verification: use only your own trusted PC and LAN.

The Copy buttons fall back to selecting the commands when clipboard access is
unavailable on an HTTP page; press Ctrl+C to copy the selected text.

The installer preserves the receiver executable bit, creates the SteamOS desktop
entry and icons, and makes PhoneCast available through the SteamVR dashboard
**+** launcher.

Only use this server on a trusted local network. Stop it with `Ctrl+C` after downloading.

## Tests

Run `npm test --prefix local-apk-server` (Node.js and Bash/curl required). Tests
cover page commands, shell syntax, browser URL substitution, download routes,
missing artifacts, and stopping after a failed curl download. Harmless fixture
tests cover bootstrap installation, platform/root rejection, missing installer,
corrupt archive, download/install failure, and temporary-directory cleanup. They do not
validate installation or browser clipboard behavior on physical Steam Frame.
