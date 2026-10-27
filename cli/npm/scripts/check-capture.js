#!/usr/bin/env node
// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

function checkCapturePackage(directory) {
  const pkg = JSON.parse(fs.readFileSync(path.join(directory, 'package.json')));
  for (const file of ['process.js', 'bin/pagx.js', 'html-snapshot/launch.js', 'html-snapshot/launch-capture.js',
    'html-snapshot/runtime.js', 'html-snapshot/capture.js', 'html-snapshot/capture-cli.js',
    'html-snapshot/snapshot.js', 'html-snapshot/ensure-built.js', 'html-snapshot/package.json',
    'html-snapshot/dist/lib/scenario-pipeline.js', 'html-snapshot/dist/lib/contact-sheet.js']) {
    if (!fs.statSync(path.join(directory, file), { throwIfNoEntry: false })?.isFile()) {
      throw new Error(`missing capture artifact: ${file}; run npm run build:html-snapshot`);
    }
  }
  const runtime = path.join(directory, 'html-snapshot');
  for (const name of ['pngjs', 'opentype.js', 'wawoff2']) require.resolve(name, { paths: [runtime] });
  const env = { ...process.env, PAGX_HTML_SNAPSHOT_NO_AUTO_INSTALL: '1' };
  for (const key of ['PAGX_NODE_BIN', 'PAGX_HTML_CAPTURE_BIN', 'PAGX_HTML_SNAPSHOT_BIN', 'PAGX_BIN']) delete env[key];
  const run = args => {
    const result = spawnSync(process.execPath, [path.join(directory, 'bin/pagx.js'), ...args], {
      cwd: directory, env, encoding: 'utf8', timeout: 15000,
    });
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(`${args.join(' ')} failed: ${result.stderr.trim()}; rebuild and stage the host native CLI`);
    return result.stdout;
  };
  const version = run(['--version']).trim();
  if (version !== `pagx ${pkg.version}`) throw new Error(`native version ${version} does not match package ${pkg.version}`);
  const help = run(['capture', '--help']);
  if (!help.includes('pagx capture') || !help.includes('--scenario') || !help.includes('--preview')) {
    throw new Error('native CLI or capture runtime is outdated: capture help is incomplete');
  }
  if (!run(['import', '--help']).includes('--checkpoint')) throw new Error('native import is outdated');
}

if (require.main === module) {
  try {
    checkCapturePackage(path.resolve(__dirname, '..'));
    process.stderr.write('check-capture: bundled runtime and host native CLI are ready.\n');
  } catch (error) {
    process.stderr.write(`check-capture: ${error.message}\n`);
    process.exitCode = 1;
  }
}
module.exports = { checkCapturePackage };
