#!/usr/bin/env node
'use strict';

// Guard executed before `npm publish` (wired via the package's
// `prepublishOnly` script, alongside check-binaries.js).
//
// The `preview/` tree powers `pagx preview` and is gitignored — it must be
// built and staged by pack.sh's stage_preview step before publishing. Its
// heavy artifacts (the single-threaded viewer wasm, the player/ext bundles)
// come out of an Emscripten build, so a release cut that forgot to run
// stage_preview would publish a package whose `pagx preview` is dead on
// arrival. package.json's "files" lists preview/, which silently ships an
// empty/partial tree rather than failing — so fail loudly here instead.
//
// Set PAGX_SKIP_PREVIEW_CHECK=1 to bypass (e.g. a deliberate CLI-only test
// publish). Not recommended for a real release.

const fs = require('fs');
const path = require('path');

const PKG_DIR = path.resolve(__dirname, '..');
const PREVIEW_DIR = path.join(PKG_DIR, 'preview');

// Key entry points and build outputs that must be present for `pagx preview`
// to run. Directories alone are not enough: an aborted stage_preview can leave
// the tree present but missing the wasm artifacts, so we check concrete files.
const REQUIRED = [
  'src/cli.js',
  'src/daemon.js',
  'src/server/index.js',
  'src/mcp/server.js',
  'static/index.html',
  'wasm/viewer/pagx-viewer.st.wasm',
  'wasm/viewer/pagx-viewer.st.esm.js',
  'wasm/player/pagx-player.esm.js',
];

function main() {
  if (process.env.PAGX_SKIP_PREVIEW_CHECK === '1') {
    process.stderr.write('check-preview: skipped (PAGX_SKIP_PREVIEW_CHECK=1)\n');
    return;
  }

  if (!fs.existsSync(PREVIEW_DIR)) {
    process.stderr.write(
      'check-preview: ERROR: preview/ directory is missing. Run pack.sh so its ' +
        'stage_preview step builds and stages the pagx-preview runtime before ' +
        '`npm publish`.\n',
    );
    process.stderr.write(
      'check-preview: set PAGX_SKIP_PREVIEW_CHECK=1 to bypass (not recommended ' +
        'for a real release).\n',
    );
    process.exit(1);
  }

  const missing = [];
  for (const rel of REQUIRED) {
    if (!fs.existsSync(path.join(PREVIEW_DIR, rel))) {
      missing.push(path.join('preview', rel));
    }
  }

  if (missing.length > 0) {
    process.stderr.write('check-preview: ERROR: preview/ is missing required artifact(s):\n');
    for (const m of missing) process.stderr.write(`  ${m}\n`);
    process.stderr.write(
      'check-preview: rebuild via pack.sh (stage_preview) so the pagx-preview ' +
        'runtime and its wasm outputs are staged before publishing.\n',
    );
    process.stderr.write(
      'check-preview: set PAGX_SKIP_PREVIEW_CHECK=1 to bypass (not recommended ' +
        'for a real release).\n',
    );
    process.exit(1);
  }

  process.stderr.write('check-preview: preview/ runtime and wasm artifacts present.\n');
}

main();
