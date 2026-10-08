const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");

const host = process.env.HOST || "0.0.0.0";
const port = Number(process.env.PORT || 8080);
const apkFileName = "phonecast-vr-v0.8-android.apk";
const defaultApkPath = path.resolve(__dirname, `../out/packages/${apkFileName}`);
const defaultChecksumsPath = path.resolve(__dirname, "../out/packages/SHA256SUMS");
const steamFrameDirectoryName = "phonecast-vr-v0.8-steam-frame-arm64";
const steamFrameFileName = `${steamFrameDirectoryName}.tar.gz`;
const defaultSteamFramePath = path.resolve(
  __dirname,
  `../out/packages/${steamFrameFileName}`,
);

function serveDownload(response, filePath, contentType, fileName, missingMessage) {
  fs.stat(filePath, (error, stats) => {
    if (error || !stats.isFile()) {
      response.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
      response.end(`${missingMessage}\n`);
      return;
    }
    response.writeHead(200, {
      "Content-Type": contentType,
      "Content-Length": stats.size,
      "Content-Disposition": `attachment; filename="${fileName}"`,
      "Cache-Control": "no-store",
    });
    fs.createReadStream(filePath).pipe(response);
  });
}

function createDownloadServer(files = {}) {
const apkPath = files.apkPath || defaultApkPath;
const steamFramePath = files.steamFramePath || defaultSteamFramePath;
const checksumsPath = files.checksumsPath || defaultChecksumsPath;
const candidateApkPath = files.candidateApkPath || path.resolve(__dirname, '../out/packages/phonecast-vr-v0.9.0-android.apk');
const candidateFramePath = files.candidateFramePath || path.resolve(__dirname, '../out/packages/phonecast-vr-v0.9.0-steam-frame-arm64.tar.gz');
const candidateChecksumsPath = files.candidateChecksumsPath || path.resolve(__dirname, '../out/packages/SHA256SUMS-v0.9.0');
return http.createServer((request, response) => {
  const url = new URL(request.url, `http://${request.headers.host || "localhost"}`);

  if (request.method !== "GET") {
    response.writeHead(405, { Allow: "GET" });
    response.end("Method not allowed\n");
    return;
  }

  if (url.pathname === "/") {
    const apkAvailable = fs.existsSync(apkPath);
    const steamFrameAvailable = fs.existsSync(steamFramePath);
    const candidateAvailable = fs.existsSync(candidateApkPath) && fs.existsSync(candidateFramePath);
    response.writeHead(apkAvailable || steamFrameAvailable || candidateAvailable ? 200 : 503, {
      "Content-Type": "text/html; charset=utf-8",
      "Cache-Control": "no-store",
    });
    response.end(`<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>PhoneCast Downloads</title>
<style>pre {background:#eee;padding:1rem;overflow:auto} button {cursor:pointer}</style></head>
<body style="font-family:sans-serif;max-width:40rem;margin:4rem auto;padding:0 1rem">
<h1>PhoneCast Downloads</h1>
<p>Early-access release: native Steam Frame receiver and release-signed Android app.</p>
${fs.existsSync(checksumsPath) ? '<p><a href="/SHA256SUMS">Download SHA256SUMS</a> to verify both downloads. Checksums from this HTTP server detect corruption, not a malicious server.</p>' : ''}
${fs.existsSync(candidateApkPath) && fs.existsSync(candidateFramePath) ? `<h2>v0.9.0 release-ready build — GitHub publication pending</h2>
<p>Owner-approved normal-use baseline: connection feedback, About/version details, safe Downloads log exports, and Find headset on LAN. Install both endpoints for discovery. Extended network/storage/lifecycle cases remain separately unvalidated. Existing v0.8 downloads and bootstrap below are unchanged.</p>
<p><a href="/phonecast-vr-v0.9.0-android.apk">Signed v0.9.0 Android APK</a> · <a href="/phonecast-vr-v0.9.0-steam-frame-arm64.tar.gz">v0.9.0 Steam Frame bundle</a> · <a href="/SHA256SUMS-v0.9.0">Candidate checksums</a></p>
<p>Stop casting before the APK update. Quit PhoneCast before the receiver update; saved settings and pairing are retained. The signing key is unchanged.</p>
<pre>set -eu
cd "$HOME/Downloads"
tar -xzf phonecast-vr-v0.9.0-steam-frame-arm64.tar.gz
sh phonecast-vr-v0.9.0-steam-frame-arm64/steam-frame-installer/install-phonecast.sh</pre>
<p>Receiver: Settings → About / Export logs. Select Export logs once to show Confirm export, then select the same row a second time to write. Phone: Settings → About. LAN search requires UDP 49322 on the same non-guest network; manual IP/pairing still works.</p>` : ''}
<h2>Android phone</h2>
${apkAvailable
  ? `<p><a href="/${apkFileName}">Download the signed Android v0.8 APK</a></p><p>If you previously installed the debug app, stop casting, note your settings, and uninstall it before installing this release: the signing keys differ. Uninstalling clears saved settings. Optional phone playback audio is off by default. Dim phone while casting is enabled by default but requires Android's separate Modify system settings approval; casting works undimmed without it.</p>`
  : "<p>The signed v0.8 APK is not available.</p>"}
<h2>Steam Frame</h2>
${steamFrameAvailable
  ? `<p><a href="/${steamFrameFileName}">Download the v0.8 Steam Frame ARM64 release bundle</a></p>
<p>On Steam Frame, open a terminal in desktop mode. Install as your normal user, without <code>sudo</code>. This is a native Linux application, not an APK.</p>
<h3>Already downloaded? Extract and install</h3>
<p>These commands assume the archive is in <code>~/Downloads</code>.</p>
<pre><code id="manual-install">set -eu
cd "$HOME/Downloads"
tar -xzf ${steamFrameFileName}
sh ${steamFrameDirectoryName}/steam-frame-installer/install-phonecast.sh</code></pre>
<button type="button" data-copy="manual-install">Copy commands</button>
<h3>Or download and install with curl</h3>
<p>Keep this PC's download server running and use the page's LAN address (not localhost). Paste this entire block into the Steam Frame terminal.</p>
<pre><code id="curl-install">curl -fSLo "$HOME/install-phonecast.sh" 'http://YOUR_PC_LAN_IP:${port}/install-phonecast.sh' &amp;&amp;
sh "$HOME/install-phonecast.sh" 'http://YOUR_PC_LAN_IP:${port}'</code></pre>
<p><a href="/install-phonecast.sh">Inspect the installer script</a> before running it if desired.</p>
<button type="button" data-copy="curl-install">Copy commands</button>
<p>The script checks Linux/ARM64 and refuses root installation. It downloads and extracts the bundle in a temporary directory, cleans up, and stops on failure. No remote script is piped into a shell. The downloaded script remains at <code>~/install-phonecast.sh</code> for inspection or removal.</p>
<p>After installation, open the SteamVR dashboard <strong>+ app launcher</strong> and select <strong>PhoneCast VR</strong>. Quit PhoneCast before updating an existing installation; settings and pairing are retained.</p>
<p>v0.8 includes in-headset settings, controller interaction, notifications, optional phone audio, and move/resize controls. Some decoder freezes and Adreno/Vulkan device-loss failures remain under investigation; save game progress before testing. This exact release archive still needs an on-headset test.</p>
<p><strong>Trusted LAN only:</strong> this development server uses unencrypted HTTP. Install only from your own trusted PC.</p>`
  : "<p>The Steam Frame ARM64 bundle has not been packaged yet.</p>"}
<script>
const curlBlock = document.getElementById('curl-install');
if (curlBlock) {
  curlBlock.textContent = curlBlock.textContent.replaceAll(
    'http://YOUR_PC_LAN_IP:${port}', window.location.origin);
}
for (const button of document.querySelectorAll('[data-copy]')) {
  button.addEventListener('click', async () => {
    const code = document.getElementById(button.dataset.copy);
    try {
      await navigator.clipboard.writeText(code.textContent);
      button.textContent = 'Copied';
    } catch {
      const range = document.createRange();
      range.selectNodeContents(code);
      const selection = window.getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
      button.textContent = 'Selected — press Ctrl+C';
    }
  });
}
</script>
</body>
</html>`);
    return;
  }

  const candidates = {
    '/phonecast-vr-v0.9.0-android.apk': [candidateApkPath, 'application/vnd.android.package-archive'],
    '/phonecast-vr-v0.9.0-steam-frame-arm64.tar.gz': [candidateFramePath, 'application/gzip'],
    '/SHA256SUMS-v0.9.0': [candidateChecksumsPath, 'text/plain; charset=utf-8'],
  };
  if (Object.hasOwn(candidates, url.pathname)) {
    const [file, type] = candidates[url.pathname];
    serveDownload(response, file, type, url.pathname.slice(1), 'Candidate artifact not found');
    return;
  }

  if (url.pathname === "/install-phonecast.sh") {
    serveDownload(response, path.join(__dirname, "install-phonecast.sh"),
      "text/plain; charset=utf-8", "install-phonecast.sh", "Installer script not found");
    return;
  }

  if (url.pathname === "/SHA256SUMS") {
    serveDownload(response, checksumsPath, "text/plain; charset=utf-8",
      "SHA256SUMS", "Checksums not found");
    return;
  }

  if (url.pathname === `/${apkFileName}` || url.pathname === "/phonecast-sender.apk") {
    serveDownload(response, apkPath, "application/vnd.android.package-archive",
      apkFileName, "Signed APK not found");
    return;
  }

  if (url.pathname === `/${steamFrameFileName}` ||
      url.pathname === "/phonecast-steam-frame-arm64.tar.gz") {
    serveDownload(response, steamFramePath, "application/gzip",
      steamFrameFileName, "Steam Frame bundle not found");
    return;
  }

  response.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
  response.end("Not found\n");
});
}

if (require.main === module) {
  const server = createDownloadServer();
  server.listen(port, host, () => {
    console.log(`PhoneCast APK server listening on http://${host}:${port}`);
    console.log(`Android APK: ${defaultApkPath}`);
    console.log(`Steam Frame bundle: ${defaultSteamFramePath}`);
  });
}

module.exports = { createDownloadServer };
