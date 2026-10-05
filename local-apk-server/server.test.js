const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');
const { spawn, spawnSync } = require('node:child_process');
const { createDownloadServer } = require('./server');

test('bootstrap success and failure paths with a harmless fixture', () => {
  const result = spawnSync('bash', [path.join(__dirname, 'installer.test.sh'),
    path.join(__dirname, 'install-phonecast.sh')], { encoding: 'utf8' });
  assert.equal(result.status, 0, result.stdout + result.stderr);
});

test('download page commands, origin substitution, routes and failure handling', async (t) => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'phonecast-download-test-'));
  const bundle = path.join(dir, 'bundle.tar.gz');
  const apk = path.join(dir, 'sender.apk');
  fs.writeFileSync(bundle, 'test bundle');
  fs.writeFileSync(apk, 'test apk');
  const server = createDownloadServer({ apkPath: apk, steamFramePath: bundle });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  t.after(async () => {
    await new Promise(resolve => server.close(resolve));
    fs.rmSync(dir, { recursive: true, force: true });
  });
  const origin = `http://127.0.0.1:${server.address().port}`;
  const page = await fetch(origin);
  assert.equal(page.status, 200);
  const html = await page.text();
  const manual = html.match(/id="manual-install">([\s\S]*?)<\/code>/)[1];
  const curl = html.match(/id="curl-install">([\s\S]*?)<\/code>/)[1].replaceAll('&amp;', '&');
  assert.match(manual, /cd "\$HOME\/Downloads"/);
  assert.match(manual, /sh phonecast-steam-frame-arm64-sprint12\/steam-frame-installer\/install-phonecast.sh/);
  assert.match(curl, /curl -fSLo/);
  assert.match(curl, /&&\nsh/);
  assert.ok(!curl.includes('| sh'));
  for (const commands of [manual, curl]) {
    const check = spawnSync('bash', ['-n'], { input: commands, encoding: 'utf8' });
    assert.equal(check.status, 0, check.stderr);
  }
  const block = { textContent: curl };
  vm.runInNewContext(html.match(/<script>([\s\S]*?)<\/script>/)[1], {
    document: { getElementById: () => block, querySelectorAll: () => [] },
    window: { location: { origin } },
  });
  assert.ok(block.textContent.includes(`'${origin}/install-phonecast.sh'`));
  assert.ok(block.textContent.includes(`'${origin}'`));
  const bootstrap = await fetch(origin + '/install-phonecast.sh');
  assert.equal(bootstrap.status, 200);
  const script = await bootstrap.text();
  assert.match(script, /aarch64\|arm64/);
  assert.match(script, /without sudo/);
  assert.equal(spawnSync('bash', ['-n'], { input: script }).status, 0);
  for (const route of ['/phonecast-steam-frame-arm64.tar.gz', '/phonecast-steam-frame-arm64-sprint12.tar.gz']) {
    const response = await fetch(origin + route);
    assert.equal(response.status, 200);
    assert.equal(await response.text(), 'test bundle');
  }
  assert.equal(await (await fetch(origin + '/phonecast-sender.apk')).text(), 'test apk');
  assert.equal((await fetch(origin, { method: 'POST' })).status, 405);
  assert.equal((await fetch(origin + '/missing')).status, 404);
  fs.unlinkSync(bundle);
  assert.equal((await fetch(origin + '/phonecast-steam-frame-arm64.tar.gz')).status, 404);
  // A failed download must stop before tar/install, with no shell-script side effects.
  const failedDownload = await new Promise((resolve, reject) => {
    const child = spawn('bash', [], { timeout: 20000 });
    let stderr = '';
    child.stderr.on('data', chunk => { stderr += chunk; });
    child.on('error', reject);
    child.on('close', status => resolve({ status, stderr }));
    child.stdin.end(block.textContent.replaceAll('$HOME', dir.replaceAll('\\', '/'))
      .replace('/install-phonecast.sh\' ', '/missing-script\' '));
  });
  assert.notEqual(failedDownload.status, 0);
  assert.match(failedDownload.stderr, /404/);
  assert.doesNotMatch(await (await fetch(origin)).text(), /id="curl-install"/);
  fs.unlinkSync(apk);
  assert.equal((await fetch(origin)).status, 503);
});
