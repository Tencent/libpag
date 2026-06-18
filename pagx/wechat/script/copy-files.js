#!/usr/bin/env node

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

/**
 * Copy wasm and js build artifacts from lib/ to wx_demo/utils/
 */
function copyFiles() {
  const sourceDir = path.resolve(__dirname, '../lib');
  const targetDir = path.resolve(__dirname, '../wx_demo/utils');

  if (!fs.existsSync(targetDir)) {
    fs.mkdirSync(targetDir, { recursive: true });
  }

  const files = ['pagx-check.js','pagx-viewer.js', 'pagx-viewer.wasm.br'];
  const missing = [];
  for (const file of files) {
    const src = path.join(sourceDir, file);
    const dst = path.join(targetDir, file);
    if (!fs.existsSync(src)) {
      missing.push(src);
      console.error(`Error: source file not found: ${src}`);
      continue;
    }
    fs.copyFileSync(src, dst);
    console.log(`Copied ${file}`);
  }
  if (missing.length > 0) {
    console.error(`${missing.length} required file(s) missing, aborting.`);
    process.exit(1);
  }
}

copyFiles();
