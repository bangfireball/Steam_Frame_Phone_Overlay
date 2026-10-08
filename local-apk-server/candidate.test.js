const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { createDownloadServer } = require('./server');
test('v0.9 candidates stay separate from v0.8 stable aliases', async t => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'phonecast-candidate-'));
  const old = path.join(dir, 'old'), apk = path.join(dir, 'apk'), frame = path.join(dir, 'frame'), sums = path.join(dir, 'sums');
  fs.writeFileSync(old, 'old'); fs.writeFileSync(apk, 'candidate apk'); fs.writeFileSync(frame, 'candidate receiver'); fs.writeFileSync(sums, 'candidate sums');
  const server = createDownloadServer({ apkPath: old, steamFramePath: old, checksumsPath: old,
    candidateApkPath: apk, candidateFramePath: frame, candidateChecksumsPath: sums });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  t.after(async () => { await new Promise(resolve => server.close(resolve)); fs.rmSync(dir, { recursive: true, force: true }); });
  const origin = `http://127.0.0.1:${server.address().port}`;
  assert.equal(await (await fetch(origin + '/phonecast-sender.apk')).text(), 'old');
  assert.equal(await (await fetch(origin + '/phonecast-steam-frame-arm64.tar.gz')).text(), 'old');
  for (const [name, body] of [['phonecast-vr-v0.9.0-android.apk','candidate apk'], ['phonecast-vr-v0.9.0-steam-frame-arm64.tar.gz','candidate receiver'], ['SHA256SUMS-v0.9.0','candidate sums']]) {
    const response = await fetch(origin + '/' + name);
    assert.equal(response.status, 200);
    assert.equal(response.headers.get('cache-control'), 'no-store');
    assert.equal(response.headers.get('content-disposition'), `attachment; filename="${name}"`);
    assert.equal(await response.text(), body);
  }
  const page = await (await fetch(origin)).text();
  assert.match(page, /GitHub publication pending/);
  assert.match(page, /same row a second time to write/);
  assert.match(page, /sh phonecast-vr-v0\.9\.0-steam-frame-arm64\/steam-frame-installer\/install-phonecast\.sh/);
  fs.unlinkSync(apk);
  assert.equal((await fetch(origin + '/phonecast-vr-v0.9.0-android.apk')).status, 404);
  assert.doesNotMatch(await (await fetch(origin)).text(), /Signed v0.9.0 Android APK/);
});
