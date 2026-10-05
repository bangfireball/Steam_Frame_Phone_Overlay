const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");

const host = process.env.HOST || "0.0.0.0";
const port = Number(process.env.PORT || 8080);
const apkPath = path.resolve(
  __dirname,
  "../android/sender/app/build/outputs/apk/debug/app-debug.apk",
);
const steamFrameFileName = "phonecast-steam-frame-arm64-sprint12.tar.gz";
const steamFramePath = path.resolve(
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

const server = http.createServer((request, response) => {
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
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>PhoneCast Downloads</title></head>
<body style="font-family:sans-serif;max-width:40rem;margin:4rem auto;padding:0 1rem">
<h1>PhoneCast Downloads</h1>
<h2>Android phone</h2>
${apkAvailable
  ? '<p><a href="/phonecast-sender.apk">Download the Android APK</a></p>'
  : "<p>The debug APK has not been built yet.</p>"}
<h2>Steam Frame</h2>
${steamFrameAvailable
  ? `<p><a href="/${steamFrameFileName}">Download the Sprint 12 ARM64 Linux bundle</a></p><p>Extract it on Steam Frame and run <code>sh steam-frame-installer/install-phonecast.sh</code>. PhoneCast will then appear in the SteamVR dashboard <strong>+</strong> launcher. It is not an APK.</p>`
  : "<p>The Steam Frame ARM64 bundle has not been packaged yet.</p>"}
</body>
</html>`);
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

server.listen(port, host, () => {
  console.log(`PhoneCast APK server listening on http://${host}:${port}`);
  console.log(`Android APK: ${apkPath}`);
  console.log(`Steam Frame bundle: ${steamFramePath}`);
});
