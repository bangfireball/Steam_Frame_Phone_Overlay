const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");

const host = process.env.HOST || "0.0.0.0";
const port = Number(process.env.PORT || 8080);
const defaultApkPath = path.resolve(
  __dirname,
  "../android/sender/app/build/outputs/apk/debug/app-debug.apk",
);
const steamFrameDirectoryName = "phonecast-steam-frame-arm64-sprint14";
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
    response.writeHead(apkAvailable || steamFrameAvailable ? 200 : 503, {
      "Content-Type": "text/html; charset=utf-8",
      "Cache-Control": "no-store",
    });
    response.end(`<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>PhoneCast Downloads</title>
<style>pre {background:#eee;padding:1rem;overflow:auto} button {cursor:pointer}</style></head>
<body style="font-family:sans-serif;max-width:40rem;margin:4rem auto;padding:0 1rem">
<h1>PhoneCast Downloads</h1>
<h2>Android phone</h2>
${apkAvailable
  ? '<p><a href="/phonecast-sender.apk">Download the current Android APK</a></p><p>The sender remains compatible with the Sprint 14 receiver and adds the home-screen Quick connect widget. Optional phone playback audio is off by default.</p>'
  : "<p>The debug APK has not been built yet.</p>"}
<h2>Steam Frame</h2>
${steamFrameAvailable
  ? `<p><a href="/${steamFrameFileName}">Download the Sprint 14 ARM64 Linux bundle</a></p>
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
<p>This Sprint 14 build adds Settings → Placement → Start Location with live dummy preview and saved distance/horizontal/vertical controls. Show keeps the phone size, resets closer to center, and faces the user. Either-stick hold toggles visibility; double click opens on that controller. Software tests pass; physical approval remains pending.</p>
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

  if (url.pathname === "/install-phonecast.sh") {
    serveDownload(response, path.join(__dirname, "install-phonecast.sh"),
      "text/plain; charset=utf-8", "install-phonecast.sh", "Installer script not found");
    return;
  }

  if (url.pathname === "/phonecast-sender.apk") {
    serveDownload(response, apkPath, "application/vnd.android.package-archive",
      "phonecast-sender-debug.apk", "APK not found");
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
