#!/usr/bin/env node

import fs from 'fs';
import path from 'path';

/**
 * Copy wasm and js build artifacts from lib/ to wx_demo/utils/
 */
function copyFiles() {
  const sourceDir = path.resolve(__dirname, '../lib');
  const targetDir = path.resolve(__dirname, '../wx_demo/utils');

  if (!fs.existsSync(targetDir)) {
    fs.mkdirSync(targetDir, { recursive: true });
  }

  const files = ['pagx-viewer.js', 'pagx-viewer.wasm.br'];
  for (const file of files) {
    const src = path.join(sourceDir, file);
    const dst = path.join(targetDir, file);
    if (!fs.existsSync(src)) {
      console.warn(`Warning: source file not found: ${src}`);
      continue;
    }
    fs.copyFileSync(src, dst);
    console.log(`Copied ${file}`);
  }
}

copyFiles();

