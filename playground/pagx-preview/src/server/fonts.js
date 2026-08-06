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

// Font source discovery for the preview server.
//
// The viewer needs a fallback font (typically NotoSansSC) plus an emoji font (NotoColorEmoji) to
// render text and emoji glyphs. This module picks a directory that holds those files, in order of
// preference:
//   1. explicit override (--fonts <dir>) passed from the CLI
//   2. PAGX_FONTS_DIR environment variable
//   3. lazy-download cache under ~/.pagx/fonts/ (populated by font-cache.js on first use)
//   4. libpag checkout's resources/font/ when running from a source tree
//
// The lazy-download step ensures a globally installed pagx-preview works out of the box; the
// libpag fallback keeps the source-tree workflow zero-config.

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { cacheDir, isFontCached, fontPath } from './font-cache.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PREVIEW_DIR = path.resolve(__dirname, '../..');

// Font role -> ordered list of preferred filenames. The first file that exists on disk is used.
export const FONT_ROLES = {
  primary: ['NotoSansSC-Regular.otf'],
  emoji: ['NotoColorEmoji.ttf'],
};

/** Walks upward looking for a libpag checkout by locating resources/font/. */
function detectResourcesFontDir() {
  let dir = PREVIEW_DIR;
  for (let i = 0; i < 6; i++) {
    const candidate = path.join(dir, 'resources', 'font');
    if (fs.existsSync(candidate) && fs.statSync(candidate).isDirectory()) {
      return candidate;
    }
    const parent = path.dirname(dir);
    if (parent === dir) break;
    dir = parent;
  }
  return null;
}

function isUsableDir(candidate) {
  return candidate && fs.existsSync(candidate) && fs.statSync(candidate).isDirectory();
}

/**
 * Resolves the fonts directory using the priority order documented at the top of this file.
 * The download cache directory qualifies only when at least one expected font is already
 * present, so a fresh install still falls through to the libpag checkout during the initial
 * download window.
 */
export function resolveFontsDir(explicitDir) {
  if (isUsableDir(explicitDir)) return path.resolve(explicitDir);
  if (isUsableDir(process.env.PAGX_FONTS_DIR)) return path.resolve(process.env.PAGX_FONTS_DIR);

  const cache = cacheDir();
  const cacheHasAny = Object.values(FONT_ROLES).some((names) =>
    names.some((name) => isFontCached(name))
  );
  if (cacheHasAny) return cache;

  const checkout = detectResourcesFontDir();
  if (checkout) return checkout;

  return null;
}

/** For each role, returns the absolute path of the first candidate file that exists, or null. */
export function listFonts(fontsDir) {
  if (!fontsDir) return {};
  const result = {};
  for (const [role, names] of Object.entries(FONT_ROLES)) {
    for (const name of names) {
      const abs = path.join(fontsDir, name);
      if (fs.existsSync(abs) && fs.statSync(abs).isFile()) {
        result[role] = { name, path: abs };
        break;
      }
    }
  }
  return result;
}

/**
 * Same as listFonts() but always uses the download cache dir when a font exists there, even if
 * the caller preferred another directory. Used for hot-swapping after a lazy download completes:
 * the /fonts endpoints resolve here so a re-request after the download picks up the new file
 * without a server restart.
 */
export function listFontsPreferringCache(fontsDir) {
  const result = listFonts(fontsDir);
  for (const [role, names] of Object.entries(FONT_ROLES)) {
    if (result[role]) continue;
    for (const name of names) {
      if (isFontCached(name)) {
        result[role] = { name, path: fontPath(name) };
        break;
      }
    }
  }
  return result;
}
