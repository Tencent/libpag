/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import express from 'express';
import path from 'path';
import fs from 'fs';
import { exec } from 'child_process';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const libpagDir = path.resolve(__dirname, '..');

const app = express();

// Enable SharedArrayBuffer (required for multi-threaded builds)
app.use((req, res, next) => {
  res.set('Cross-Origin-Opener-Policy', 'same-origin');
  res.set('Cross-Origin-Embedder-Policy', 'require-corp');
  next();
});

// Map /fonts to resources/font directory
app.use('/fonts', express.static(path.join(libpagDir, 'resources', 'font')));

// Generate samples/index.json dynamically for development
app.get('/samples/index.json', (req, res) => {
  const samplesDir = path.join(libpagDir, 'spec', 'samples');
  const files = fs.readdirSync(samplesDir)
    .filter(f => f.endsWith('.pagx'))
    .sort();
  res.json(files);
});

// Map /samples to spec/samples directory
app.use('/samples', express.static(path.join(libpagDir, 'spec', 'samples')));

app.use('', express.static(__dirname, {
  setHeaders: (res, filePath) => {
    if (filePath.endsWith('.wasm')) {
      res.set('Content-Type', 'application/wasm');
    }
  }
}));

app.get('/', (req, res) => {
  res.redirect('/index.html');
});

const port = 8080;
app.listen(port, () => {
  const url = `http://localhost:${port}/`;
  const start = (process.platform === 'darwin' ? 'open' : 'start');
  exec(start + ' ' + url);
  console.log(`PAGX Playground running at ${url}`);
});
