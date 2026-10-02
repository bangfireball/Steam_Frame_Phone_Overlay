const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");

const host = process.env.HOST || "0.0.0.0";
const port = Number(process.env.PORT || 8080);
const apkPath = path.resolve(
  __dirname,
  "../android/sender/app/build/outputs/apk/debug/app-debug.apk",
);

const server = http.createServer((request, response) => {
  const url = new URL(request.url, `http://${request.headers.host || "localhost"}`);

  if (request.method !== "GET") {
    response.writeHead(405, { Allow: "GET" });
    response.end("Method not allowed\n");
    return;
  }

  if (url.pathname === "/") {
    const available = fs.existsSync(apkPath);
    response.writeHead(available ? 200 : 503, {
      "Content-Type": "text/html; charset=utf-8",
      "Cache-Control": "no-store",
    });
    response.end(`<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>PhoneCast APK</title></head>
<body style="font-family:sans-serif;max-width:40rem;margin:4rem auto;padding:0 1rem">
<h1>PhoneCast Sender</h1>
${available
  ? '<p><a href="/phonecast-sender.apk">Download the Android APK</a></p>'
  : "<p>The debug APK has not been built yet.</p>"}
</body>
</html>`);
    return;
  }

  if (url.pathname === "/phonecast-sender.apk") {
    fs.stat(apkPath, (error, stats) => {
      if (error || !stats.isFile()) {
        response.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
        response.end("APK not found\n");
        return;
      }
      response.writeHead(200, {
        "Content-Type": "application/vnd.android.package-archive",
        "Content-Length": stats.size,
        "Content-Disposition": 'attachment; filename="phonecast-sender-debug.apk"',
        "Cache-Control": "no-store",
      });
      fs.createReadStream(apkPath).pipe(response);
    });
    return;
  }

  response.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
  response.end("Not found\n");
});

server.listen(port, host, () => {
  console.log(`PhoneCast APK server listening on http://${host}:${port}`);
  console.log(`Serving ${apkPath}`);
});
