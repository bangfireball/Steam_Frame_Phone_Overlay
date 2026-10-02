# Local APK server

Optional, dependency-free Node.js server for downloading the latest debug APK over the local network. It is not part of PhoneCast runtime or build architecture.

From the repository root:

```powershell
node local-apk-server/server.js
```

It listens on port `8080` by default and serves the current Android build directly from:

```text
android/sender/app/build/outputs/apk/debug/app-debug.apk
```

Override the port if needed:

```powershell
$env:PORT=8081
node local-apk-server/server.js
```

Only use this on a trusted local network. Stop it with `Ctrl+C` when the APK has been downloaded.
