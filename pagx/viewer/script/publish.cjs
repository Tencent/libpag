#!/usr/bin/env node
/**
 * PAGX Viewer Publisher
 *
 * Builds and copies the PAGX Viewer to the public directory.
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
const { execSync } = require('child_process');

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
 * Run a command and print output.
 */
function runCommand(command, cwd) {
  console.log(`  Running: ${command}`);
  execSync(command, { cwd, stdio: 'inherit' });
}

/**
 * Main function.
 */
function main() {
  console.log('Publishing PAGX Viewer...\n');

  // Clean and rebuild
  console.log('Step 1: Clean build...');
  runCommand('npm run clean', VIEWER_DIR);

  console.log('\nStep 2: Build release...');
  runCommand('npm run build:release', VIEWER_DIR);

  // Copy index.html
  console.log('\nStep 3: Copy files...');
  copyFile(
    path.join(VIEWER_DIR, 'index.html'),
    path.join(OUTPUT_DIR, 'index.html')
  );

  // Copy index.css
  copyFile(
    path.join(VIEWER_DIR, 'index.css'),
    path.join(OUTPUT_DIR, 'index.css')
  );

  // Copy favicon and logo
  copyFile(
    path.join(VIEWER_DIR, 'favicon.png'),
    path.join(OUTPUT_DIR, 'favicon.png')
  );
  copyFile(
    path.join(VIEWER_DIR, 'logo.png'),
    path.join(OUTPUT_DIR, 'logo.png')
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
  const wasmDir = path.join(VIEWER_DIR, 'wasm-mt');
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
