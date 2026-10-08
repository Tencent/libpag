#!/usr/bin/env node
'use strict';

// Dependency-free so the shell driver can reject LFS pointers before npm bootstrap.
const fs = require('fs');
const path = require('path');

function checkAssets(manifest, labels, repoRoot) {
  const visited = new Set();
  const pointers = [];
  const visit = (file) => {
    file = path.resolve(repoRoot, file);
    if (visited.has(file)) return;
    visited.add(file);
    if (fs.statSync(file).isDirectory()) {
      for (const name of fs.readdirSync(file)) visit(path.join(file, name));
      return;
    }
    const fd = fs.openSync(file, 'r');
    try {
      const header = Buffer.alloc(128);
      fs.readSync(fd, header, 0, header.length, 0);
      if (header.toString('utf8').startsWith('version https://git-lfs.github.com/spec/v1')) {
        pointers.push(path.relative(repoRoot, file));
      }
    } finally {
      fs.closeSync(fd);
    }
  };
  for (const label of labels) {
    const corpus = manifest.corpora && manifest.corpora[label];
    if (!corpus) throw new Error(`unknown corpus '${label}'`);
    for (const root of corpus.roots || []) visit(typeof root === 'string' ? root : root.path);
    for (const resource of corpus.resources || []) visit(resource);
    for (const deck of corpus.decks || []) {
      for (const input of deck.inputs) visit(input);
    }
  }
  visit('resources/font');
  if (pointers.length) {
    throw new Error(`Git LFS assets are not materialized:\n${pointers.join('\n')}\nRun 'git lfs pull' before PPT evaluation.`);
  }
}

if (require.main === module) {
  try {
    const [manifestPath, ...labels] = process.argv.slice(2);
    const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
    checkAssets(manifest, labels, path.resolve(__dirname, '../..'));
  } catch (error) {
    console.error(`ppt-preflight: ${error.message}`);
    process.exitCode = 1;
  }
}

module.exports = { checkAssets };
