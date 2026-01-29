#!/usr/bin/env node
/**
 * PAGX Viewer Publisher
 *
 * Copies the PAGX Viewer build output to the public directory.
 * Requires release build (npm run build:release).
 *
 * Source files:
 *     index.html
 *     index.css
 *     wasm-mt/
 *     ../../resources/font/  (from libpag root)
 *
 * Output structure:
 *     ../public/viewer/index.html
 *     ../public/viewer/index.css
 *     ../public/viewer/fonts/
 *     ../public/viewer/wasm-mt/
 *
 * Usage:
 *     npm run publish
 */

const fs = require('fs');
const path = require('path');

// Default paths
const SCRIPT_DIR = __dirname;
const VIEWER_DIR = path.dirname(SCRIPT_DIR);
const PAGX_DIR = path.dirname(VIEWER_DIR);
const LIBPAG_DIR = path.dirname(PAGX_DIR);
const RESOURCES_FONT_DIR = path.join(LIBPAG_DIR, 'resources', 'font');
const PUBLIC_DIR = path.join(PAGX_DIR, 'public');
const OUTPUT_DIR = path.join(PUBLIC_DIR, 'viewer');

/**
 * Copy a file from source to destination.
 */
function copyFile(src, dest) {
  const destDir = path.dirname(dest);
  fs.mkdirSync(destDir, { recursive: true });
  fs.copyFileSync(src, dest);
  console.log(`  Copied: ${path.relative(VIEWER_DIR, dest)}`);
}

/**
 * Check if the build is a release build (minified, no sourcemaps).
 */
function checkReleaseBuild(wasmDir) {
  const indexJs = path.join(wasmDir, 'index.js');

  // Check if index.js exists
  if (!fs.existsSync(indexJs)) {
    return { ok: false, reason: 'wasm-mt/index.js not found. Please run "npm run build:release" first.' };
  }

  // Check if sourcemap exists (should not exist in release)
  const mapFile = path.join(wasmDir, 'index.js.map');
  if (fs.existsSync(mapFile)) {
    return { ok: false, reason: 'Sourcemap file found. Please run "npm run build:release" first.' };
  }

  // Check if JS is minified (first line should be very long)
  const content = fs.readFileSync(indexJs, 'utf-8');
  const firstLine = content.split('\n')[0];
  if (firstLine.length < 1000) {
    return { ok: false, reason: 'JS file does not appear to be minified. Please run "npm run build:release" first.' };
  }

  return { ok: true };
}

/**
 * Main function.
 */
function main() {
  console.log('Publishing PAGX Viewer...\n');

  // Check if viewer build exists
  const wasmDir = path.join(VIEWER_DIR, 'wasm-mt');
  if (!fs.existsSync(wasmDir)) {
    console.error('Error: Viewer build not found. Run "npm run build:release" first.');
    process.exit(1);
  }

  // Check if it's a release build
  const releaseCheck = checkReleaseBuild(wasmDir);
  if (!releaseCheck.ok) {
    console.error(`Error: ${releaseCheck.reason}`);
    process.exit(1);
  }

  // Copy index.html
  copyFile(
    path.join(VIEWER_DIR, 'index.html'),
    path.join(OUTPUT_DIR, 'index.html')
  );

  // Copy index.css
  copyFile(
    path.join(VIEWER_DIR, 'index.css'),
    path.join(OUTPUT_DIR, 'index.css')
  );

  // Copy fonts from resources/font
  console.log('\n  Copying fonts...');
  copyFile(
    path.join(RESOURCES_FONT_DIR, 'NotoSansSC-Regular.otf'),
    path.join(OUTPUT_DIR, 'fonts', 'NotoSansSC-Regular.otf')
  );
  copyFile(
    path.join(RESOURCES_FONT_DIR, 'NotoColorEmoji.ttf'),
    path.join(OUTPUT_DIR, 'fonts', 'NotoColorEmoji.ttf')
  );

  // Copy wasm-mt directory
  console.log('\n  Copying wasm-mt...');
  const wasmOutputDir = path.join(OUTPUT_DIR, 'wasm-mt');
  copyFile(
    path.join(wasmDir, 'index.js'),
    path.join(wasmOutputDir, 'index.js')
  );
  copyFile(
    path.join(wasmDir, 'pagx-viewer.js'),
    path.join(wasmOutputDir, 'pagx-viewer.js')
  );
  copyFile(
    path.join(wasmDir, 'pagx-viewer.wasm'),
    path.join(wasmOutputDir, 'pagx-viewer.wasm')
  );

  console.log('\nDone!');
}

main();
