# Local PhoneCast download server

Optional, dependency-free Node.js server for downloading PhoneCast artifacts over the trusted local network. It is not part of the runtime architecture.

From the repository root:

```powershell
node local-apk-server/server.js
```

It listens on port `8080` by default and offers whichever artifacts currently exist:

- Android APK: `android/sender/app/build/outputs/apk/debug/app-debug.apk`
- Steam Frame ARM64 Sprint 12 bundle: `out/packages/phonecast-steam-frame-arm64-sprint12.tar.gz`

Override the port if needed:

```powershell
$env:PORT=8081
node local-apk-server/server.js
```

## Package the Steam Frame bundle

First complete the Linux ARM64 build and install staging described in `docs/development.md`. Then package the native receiver and adjacent OpenVR assets:

```bash
mkdir -p out/packages
tar -czf out/packages/phonecast-steam-frame-arm64-sprint12.tar.gz \
  -C out/packages phonecast-steam-frame-arm64-sprint12
```

The archive is a native AArch64 Linux application, not an APK. Extract it on
Steam Frame and run:

```bash
sh phonecast-steam-frame-arm64-sprint12/steam-frame-installer/install-phonecast.sh
```

The installer preserves the receiver executable bit, creates the SteamOS desktop
entry and icons, and makes PhoneCast available through the SteamVR dashboard
**+** launcher.

Only use this server on a trusted local network. Stop it with `Ctrl+C` after downloading.
