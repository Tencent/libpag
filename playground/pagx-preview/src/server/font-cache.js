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

// Lazy-download the fallback fonts (NotoSansSC + NotoColorEmoji) into ~/.pagx/fonts/ so a
// globally installed pagx-preview works without a libpag checkout. Modeled after
// cli/npm/html-snapshot/launch.js: the download runs on first use, output goes to stderr, and
// PAGX_FONTS_NO_AUTO_DOWNLOAD=1 disables it for offline / CI hosts.

import fs from 'fs';
import os from 'os';
import path from 'path';
import https from 'https';
import http from 'http';

const CACHE_DIR = path.join(os.homedir(), '.pagx', 'fonts');
// Any file smaller than this is considered a failed download (e.g. HTML error page) and will
// be retried on the next startup. Real NotoSansSC / NotoColorEmoji are ~8 MB / ~10 MB.
const MIN_VALID_SIZE = 100 * 1024;

// The catalog mirrors the CDN used by pagx/wechat/wx_demo, which is where pag.qq.com already
// serves the two fonts pagx-viewer needs. Bumping the URL here is the only step required to
// switch to a different origin later.
export const FONT_CATALOG = [
  {
    role: 'primary',
    name: 'NotoSansSC-Regular.otf',
    url: 'https://pag.qq.com/wx_pagx_demo/fonts/NotoSansSC-Regular.otf',
  },
  {
    role: 'emoji',
    name: 'NotoColorEmoji.ttf',
    url: 'https://pag.qq.com/wx_pagx_demo/fonts/NotoColorEmoji.ttf',
  },
];

export function cacheDir() {
  return CACHE_DIR;
}

function ensureCacheDir() {
  fs.mkdirSync(CACHE_DIR, { recursive: true });
}

function log(msg) {
  process.stderr.write(`pagx-preview font-cache: ${msg}\n`);
}

/** Returns true when a cached font file exists and looks intact. */
export function isFontCached(name) {
  const abs = path.join(CACHE_DIR, name);
  try {
    const stat = fs.statSync(abs);
    return stat.isFile() && stat.size >= MIN_VALID_SIZE;
  } catch (_) {
    return false;
  }
}

/** Returns absolute path to the cached font (regardless of validity). */
export function fontPath(name) {
  return path.join(CACHE_DIR, name);
}

/** Downloads a single URL to `destPath`, following up to 3 redirects. Rejects on non-2xx. */
function downloadOnce(url, destPath, maxRedirects = 3) {
  return new Promise((resolve, reject) => {
    const client = url.startsWith('https:') ? https : http;
    const tmpPath = destPath + '.part';
    const file = fs.createWriteStream(tmpPath);
    const req = client.get(url, (res) => {
      if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location && maxRedirects > 0) {
        // Follow redirect. Close the file (it will be overwritten on the next attempt).
        file.close();
        fs.unlink(tmpPath, () => {});
        downloadOnce(res.headers.location, destPath, maxRedirects - 1).then(resolve, reject);
        return;
      }
      if (res.statusCode !== 200) {
        file.close();
        fs.unlink(tmpPath, () => {});
        reject(new Error(`HTTP ${res.statusCode} for ${url}`));
        return;
      }
      res.pipe(file);
      file.on('finish', () => {
        file.close((err) => {
          if (err) {
            fs.unlink(tmpPath, () => {});
            reject(err);
            return;
          }
          try {
            const stat = fs.statSync(tmpPath);
            if (stat.size < MIN_VALID_SIZE) {
              fs.unlink(tmpPath, () => {});
              reject(new Error(`downloaded file too small (${stat.size} bytes)`));
              return;
            }
            fs.renameSync(tmpPath, destPath);
            resolve();
          } catch (e) {
            fs.unlink(tmpPath, () => {});
            reject(e);
          }
        });
      });
    });
    req.on('error', (err) => {
      file.close();
      fs.unlink(tmpPath, () => {});
      reject(err);
    });
    req.setTimeout(30000, () => {
      req.destroy(new Error('download timed out'));
    });
  });
}

// Tracks per-role in-flight downloads so overlapping requests coalesce. First call kicks off the
// download; concurrent callers await the same promise.
const inFlight = new Map();

/**
 * Ensures a font exists in the cache. Returns absolute path on success, null on failure. Safe to
 * call from multiple places at once — downloads are deduplicated.
 */
export async function ensureFont(entry) {
  if (isFontCached(entry.name)) return fontPath(entry.name);
  if (process.env.PAGX_FONTS_NO_AUTO_DOWNLOAD === '1') {
    log(`auto-download disabled (PAGX_FONTS_NO_AUTO_DOWNLOAD=1); skipping ${entry.name}`);
    return null;
  }

  let promise = inFlight.get(entry.name);
  if (!promise) {
    promise = (async () => {
      ensureCacheDir();
      const dest = fontPath(entry.name);
      log(`downloading ${entry.name} from ${entry.url}...`);
      const started = Date.now();
      try {
        await downloadOnce(entry.url, dest);
        const secs = ((Date.now() - started) / 1000).toFixed(1);
        log(`downloaded ${entry.name} (${secs}s)`);
        return dest;
      } catch (err) {
        log(`failed to download ${entry.name}: ${err.message}`);
        return null;
      }
    })();
    inFlight.set(entry.name, promise);
    // Clear the entry once resolved so a later retry can try again.
    promise.finally(() => inFlight.delete(entry.name));
  }
  return await promise;
}

/** Downloads any missing font in parallel. Returns a map of role -> absolute path (or null). */
export async function ensureAllFonts() {
  const results = await Promise.all(FONT_CATALOG.map(async (entry) => ({
    role: entry.role,
    name: entry.name,
    path: await ensureFont(entry),
  })));
  const map = {};
  for (const r of results) map[r.role] = r;
  return map;
}
