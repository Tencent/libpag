/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
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
import { exec } from 'child_process';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();

// Enable SharedArrayBuffer
app.use((req, res, next) => {
  res.set('Cross-Origin-Opener-Policy', 'same-origin');
  res.set('Cross-Origin-Embedder-Policy', 'require-corp');
  next();
});

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

const port = 8082;
app.listen(port, () => {
  const url = `http://localhost:${port}/`;
  const start = (process.platform === 'darwin' ? 'open' : 'start');
  exec(start + ' ' + url);
  console.log(`PAGX Viewer running at ${url}`);
});
