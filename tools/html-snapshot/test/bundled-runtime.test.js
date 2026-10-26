// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFile, spawn } = require('child_process');
const { promisify } = require('util');
const exec = promisify(execFile);
let dir, runtime, env, calls;
beforeEach(() => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), "browser bootstrap 中文 ' "));
  const pkg = path.join(dir, 'package');
  fs.mkdirSync(path.join(pkg, 'html-snapshot'), { recursive: true });
  runtime = path.join(pkg, 'html-snapshot/runtime.js');
  fs.copyFileSync(path.resolve(__dirname, '../../../cli/npm/html-snapshot/runtime.js'), runtime);
  fs.copyFileSync(path.resolve(__dirname, '../../../cli/npm/process.js'), path.join(pkg, 'process.js'));
  fs.writeFileSync(path.join(pkg, 'html-snapshot/package.json'), JSON.stringify({
    browserDependencies: { puppeteer: '^23.0.0', playwright: '^1.60.0' },
  }));
  calls = path.join(dir, 'calls.jsonl');
  const npm = path.join(dir, 'npm-cli.js');
  fs.writeFileSync(npm, `
    const fs = require('fs'), path = require('path');
    const args = process.argv.slice(2);
    fs.appendFileSync(process.env.CAPTURE_NPM_CALLS, JSON.stringify(args) + '\\n');
    if (process.env.CAPTURE_INSTALL_FAIL) process.exit(17);
    const engine = args[0] === 'install' ? args.at(-1).split('@')[0] : args[2];
    const browser = path.join(process.env[engine === 'puppeteer' ? 'PUPPETEER_CACHE_DIR' : 'PLAYWRIGHT_BROWSERS_PATH'], 'browser');
    if (args[0] === 'install') {
      const folder = path.join(process.cwd(), 'node_modules', engine);
      fs.mkdirSync(folder, { recursive: true });
      fs.writeFileSync(path.join(folder, 'index.js'), (engine === 'playwright' ? 'exports.chromium = ' : 'module.exports = ')
        + '{ executablePath: () => ' + JSON.stringify(browser) + ' };');
    } else {
      fs.mkdirSync(path.dirname(browser), { recursive: true }); fs.writeFileSync(browser, 'browser fixture');
    }
  `);
  env = { ...process.env, NODE_PATH: '', npm_execpath: npm, CAPTURE_NPM_CALLS: calls,
    PAGX_HTML_SNAPSHOT_CACHE: path.join(dir, 'cache'), PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL: '0' };
  for (const key of ['PUPPETEER_CACHE_DIR', 'PLAYWRIGHT_BROWSERS_PATH', 'PUPPETEER_EXECUTABLE_PATH', 'PLAYWRIGHT_EXECUTABLE_PATH']) delete env[key];
});
afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));
const prepare = (engine, extra = {}) => exec(process.execPath, ['-e', `
  require(process.argv[1]).prepareBrowser(process.argv[2]).then(env => console.log(JSON.stringify(env.NODE_PATH)))
    .catch(error => { console.error(error.message); process.exitCode = error.exitCode || 1; });
`, runtime, engine], { env: { ...env, ...extra }, timeout: 10000 });

test.each(['puppeteer', 'playwright'])('lazy bootstrap installs only %s and reuses the matching cache', async engine => {
  const result = await prepare(engine);
  expect(result.stdout).toContain(path.join(dir, 'cache/node_modules'));
  const installed = fs.readFileSync(calls, 'utf8').trim().split('\n').map(JSON.parse);
  expect(installed).toHaveLength(2);
  expect(installed[0].at(-1)).toBe(engine === 'puppeteer' ? 'puppeteer@^23.0.0' : 'playwright@^1.60.0');
  expect(installed[1]).toEqual(['exec', '--', engine, ...(engine === 'puppeteer' ? ['browsers', 'install', 'chrome'] : ['install', 'chromium'])]);
  await prepare(engine, { PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL: '1' });
  expect(fs.readFileSync(calls, 'utf8').trim().split('\n')).toHaveLength(2);
});

test('installation failures retain their exit code and disabled installation does not mutate the cache', async () => {
  await expect(prepare('puppeteer', { PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL: '1' }))
    .rejects.toMatchObject({ code: 1, stderr: expect.stringContaining('auto-install disabled') });
  expect(fs.existsSync(env.PAGX_HTML_SNAPSHOT_CACHE)).toBe(false);
  await expect(prepare('puppeteer', { CAPTURE_INSTALL_FAIL: '1' }))
    .rejects.toMatchObject({ code: 17, stderr: expect.stringContaining('exit 17') });
});

(process.platform === 'win32' ? test.skip : test)('launcher relays termination and waits for child cleanup', async () => {
  const marker = path.join(dir, 'cleaned');
  const launcher = path.join(dir, 'launcher.js');
  const processHelper = path.resolve(__dirname, '../../../cli/npm/process.js');
  fs.writeFileSync(launcher, `require(${JSON.stringify(processHelper)}).runProcess(process.execPath, ['-e',
    ${JSON.stringify("process.on('SIGTERM', () => { require('fs').writeFileSync(process.argv[1], 'done'); process.exit(0); }); console.log('ready'); setInterval(() => {}, 1000);")},
    ${JSON.stringify(marker)}]).then(code => { process.exitCode = code; });`);
  const child = spawn(process.execPath, [launcher], { stdio: ['ignore', 'pipe', 'pipe'] });
  const timeout = setTimeout(() => child.kill('SIGKILL'), 8000);
  child.stdout.once('data', () => child.kill('SIGTERM'));
  try {
    const code = await new Promise((resolve, reject) => { child.on('close', resolve); child.on('error', reject); });
    expect(code).toBe(143);
    expect(fs.readFileSync(marker, 'utf8')).toBe('done');
  } finally { clearTimeout(timeout); }
});
