#!/usr/bin/env node

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const projectRoot = path.resolve(__dirname, '..');

const sourceDir = path.join(projectRoot, 'wx_demo', 'utils');
const targetDir = '/Users/billyjin/Desktop/project/cocraft-wechat/packages/frontend/miniprogram/dist/packageFile/utils';

const files = ['pagx-viewer.js', 'pagx-viewer.wasm.br'];

if (!fs.existsSync(targetDir)) {
  fs.mkdirSync(targetDir, { recursive: true });
  console.log(`Created target directory: ${targetDir}`);
}

for (const file of files) {
  const src = path.join(sourceDir, file);
  const dst = path.join(targetDir, file);
  if (!fs.existsSync(src)) {
    console.error(`Source file not found: ${src}`);
    process.exit(1);
  }
  fs.copyFileSync(src, dst);
  const size = fs.statSync(dst).size;
  console.log(`Copied ${file} (${(size / 1024).toFixed(1)} KB) -> ${dst}`);
}

console.log('All files copied to cocraft successfully.');
