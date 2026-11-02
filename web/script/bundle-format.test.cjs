const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const libDir = path.resolve(__dirname, '..', process.env.PAG_LIB_DIR || 'lib');
const { TARGETS, resolveTarget } = require('./targets');

for (const target of TARGETS) {
  test(`the ${target.wasmDir} entry rewrite completes before Rollup can read it`, () => {
    let entry = "import createPAG from './wasm/libpag';\nexport { createPAG };\n";
    const source = fs.readFileSync(path.resolve(__dirname, 'update.pag.import.js'), 'utf8');
    const mockFs = {
      readFileSync() {
        return entry;
      },
      writeFileSync(filename, content) {
        entry = content;
      },
      readFile(filename, encoding, callback) {
        callback(null, entry);
      },
      writeFile() {
        entry = '';
      },
    };
    const context = {
      __dirname,
      process: { env: { ARCH: target.arch, BACKEND: target.backend } },
      console: { log() {} },
      require(name) {
        if (name === 'fs') return mockFs;
        if (name === 'path') return path;
        if (name === './targets') return { resolveTarget };
        throw new Error(`Unexpected import: ${name}`);
      },
    };
    new vm.Script(source).runInNewContext(context);
    assert.equal(entry, `import createPAG from './${target.wasmDir}/libpag';\nexport { createPAG };\n`);
  });
}

for (const filename of ['libpag.umd.js', 'libpag.min.js', 'libpag.cjs.js']) {
  test(`${filename} parses without ES-module syntax`, () => {
    const source = fs.readFileSync(path.join(libDir, filename), 'utf8');
    assert.doesNotThrow(() => new vm.Script(source, { filename }));
  });
}

for (const filename of ['libpag.umd.js', 'libpag.min.js']) {
  test(`${filename} captures its script URL before asynchronous initialization`, async () => {
    const scriptUrl = `https://example.test/lib/${filename}`;
    const requests = [];
    const sandbox = {
      URL,
      TextDecoder,
      TextEncoder,
      AbortController,
      WeakRef,
      FinalizationRegistry,
      WebAssembly,
      console: { log() {}, warn() {}, error() {} },
      navigator: { userAgent: 'bundle-format-test' },
      location: { href: 'https://example.test/cypress/index.html' },
      document: {
        currentScript: { src: scriptUrl, tagName: 'SCRIPT' },
        baseURI: 'https://example.test/cypress/index.html',
      },
      fetch(url) {
        requests.push(String(url));
        return Promise.reject(new Error('Fetch intercepted by the URL test'));
      },
    };
    sandbox.window = sandbox;
    sandbox.self = sandbox;
    const context = vm.createContext(sandbox);
    const source = fs.readFileSync(path.join(libDir, filename), 'utf8');
    new vm.Script(source, { filename }).runInContext(context);
    assert.equal(typeof context.libpag.PAGInit, 'function');
    context.document.currentScript = null;
    await assert.rejects(context.libpag.PAGInit(), /PAGInit fail/);
    assert.ok(requests.length > 0);
    assert.ok(requests.every((url) => url === 'https://example.test/lib/libpag.wasm'));
  });
}

test('the Cypress page loads the UMD bundle as a classic script', () => {
  const page = fs.readFileSync(path.resolve(__dirname, '../cypress/index.html'), 'utf8');
  const script = page.match(/<script\b([^>]*\bsrc=["']\.\.\/lib\/libpag\.umd\.js["'][^>]*)>/i);
  assert.ok(script, 'Cypress must load the WebGL UMD bundle');
  assert.doesNotMatch(script[1], /\btype\s*=\s*["']module["']/i);
});
