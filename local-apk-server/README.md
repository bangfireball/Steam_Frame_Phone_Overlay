# Local PhoneCast release download server

Optional dependency-free Node.js server for downloading the v0.8 release assets over a trusted local network. It is not part of the phone-to-headset runtime architecture and is not a public Internet service.

From the repository root:

```powershell
node local-apk-server/server.js
```

It listens on port `8080` by default and serves:

| File | Local source | Download route |
| --- | --- | --- |
| Signed Android v0.8 APK | `out/packages/phonecast-vr-v0.8-android.apk` | `/phonecast-vr-v0.8-android.apk` |
| Steam Frame ARM64 release | `out/packages/phonecast-vr-v0.8-steam-frame-arm64.tar.gz` | `/phonecast-vr-v0.8-steam-frame-arm64.tar.gz` |
| Checksums for both assets | `out/packages/SHA256SUMS` | `/SHA256SUMS` |

The existing `/phonecast-sender.apk` and `/phonecast-steam-frame-arm64.tar.gz` aliases serve these same release files. There is no fallback to debug APKs or older sprint bundles. Missing assets return 404; the page reports their absence. Old sprint-specific routes are no longer served.

The signed APK cannot update a debug-signed installation. Stop casting, note the saved settings, uninstall the debug app, and then install the release APK; uninstalling clears saved app settings. Future public APK updates must use the same release signing key. Never serve or commit keystores or private signing properties.

Override the port if needed:

```powershell
$env:PORT=8081
node local-apk-server/server.js
```

Open `http://YOUR_PC_LAN_IP:8080` from the phone/headset. Do not use `localhost` on another device. HTTP is unencrypted and does not authenticate artifacts; checksums from the same server detect corruption, not a malicious server. Stop the server with Ctrl+C after downloading.

## v0.9.0 versioned release assets

When both artifacts exist, the page exposes a separately labelled **release-ready build — GitHub publication pending** section:

- `/phonecast-vr-v0.9.0-android.apk`
- `/phonecast-vr-v0.9.0-steam-frame-arm64.tar.gz`
- `/SHA256SUMS-v0.9.0`

These read matching files from `out/packages` and never replace the v0.8 aliases or bootstrap. Install both endpoints to exercise discovery; stop casting/quit the receiver before updating. The candidate uses the same APK signing key. Manual commands extract the `phonecast-vr-v0.9.0-steam-frame-arm64` directory and run its included installer. Do **not** use the old bootstrap to install the candidate.

About/build details, safe Downloads exports and the IPv4 UDP finder are documented in `docs/v0.9-support.md`. The owner accepted the normal-use release baseline, not every extended network/storage/lifecycle case. Local serving does not publish a GitHub release. The headset Export logs row needs two deliberate selections: first to show Confirm export, second to write. Server tests cover versioned candidate routes, missing files, no-store headers, and separation from the stable aliases.

## Steam Frame install

The page provides copyable manual and curl-based install commands. For an archive already downloaded to Steam Frame:

```bash
set -eu
cd "$HOME/Downloads"
tar -xzf phonecast-vr-v0.8-steam-frame-arm64.tar.gz
sh phonecast-vr-v0.8-steam-frame-arm64/steam-frame-installer/install-phonecast.sh
```

Or download and inspect the bootstrap before running it:

```bash
curl -fSLo "$HOME/install-phonecast.sh" 'http://YOUR_PC_LAN_IP:8080/install-phonecast.sh' &&
sh "$HOME/install-phonecast.sh" 'http://YOUR_PC_LAN_IP:8080'
```

Run as the normal headset user, without `sudo`. Quit PhoneCast before updating. The bootstrap checks Linux/ARM64, refuses root, downloads the release via the stable archive alias, extracts into a temporary directory, stops on failure, and cleans up. It does not pipe a remote script into a shell. The downloaded bootstrap remains at `~/install-phonecast.sh` for inspection/removal. Settings and pairing survive receiver reinstalls.

After installing, open the SteamVR dashboard **+** launcher and select **PhoneCast VR**. This exact release archive still needs an on-headset test; native decoder/GPU reliability limitations remain in the root README.

Copy buttons fall back to selecting text if the browser denies clipboard access on HTTP; press Ctrl+C to copy it.

## Updating assets and server code

The server reads artifacts on each request and sends `Cache-Control: no-store`. Replacing an APK/archive/checksum file needs no restart; regenerate `SHA256SUMS` after changing either asset. Changing server code requires restarting this Node process, not SteamVR or PhoneCast.

Before sharing, run `sha256sum -c SHA256SUMS` from `out/packages`. Keep Linux installer scripts/checksum files LF-terminated, archive executable bits preserved, and the archive's top-level directory named `phonecast-vr-v0.8-steam-frame-arm64` to match the bootstrap.

## Tests

```powershell
npm test --prefix local-apk-server
```

Requires Node.js and Bash/curl. Tests cover versioned and stable routes, release filenames, no-store headers, checksum availability, page commands, browser URL substitution, shell syntax, missing assets, and failed downloads. Harmless fixtures exercise bootstrap installation, platform/root rejection, missing installer, corrupt archive, download/install failure, and temporary-directory cleanup. No real receiver or physical headset is started by these tests.
