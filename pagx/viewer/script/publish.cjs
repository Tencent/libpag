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
 *     <output>/index.html
 *     <output>/index.css
 *     <output>/fonts/
 *     <output>/wasm-mt/
 *
 * Usage:
 *     npm run publish [-- -o <output-dir>]
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
const DEFAULT_OUTPUT_DIR = path.join(PAGX_DIR, 'public');

/**
 * Parse command line arguments.
 */
function parseArgs() {
  const args = process.argv.slice(2);
  let outputDir = DEFAULT_OUTPUT_DIR;

  for (let i = 0; i < args.length; i++) {
    if ((args[i] === '-o' || args[i] === '--output') && args[i + 1]) {
      outputDir = path.resolve(args[i + 1]);
      i++;
    } else if (args[i] === '-h' || args[i] === '--help') {
      console.log(`
PAGX Viewer Publisher

Usage:
    npm run publish [-- -o <output-dir>]

Options:
    -o, --output <dir>  Output directory (default: ../public/viewer)
    -h, --help          Show this help message
`);
      process.exit(0);
    }
  }

  return { outputDir };
}

/**
 * Copy a file from source to destination.
 */
function copyFile(src, dest) {
  const destDir = path.dirname(dest);
  fs.mkdirSync(destDir, { recursive: true });
  fs.copyFileSync(src, dest);
  console.log(`  Copied: ${dest}`);
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
  const { outputDir } = parseArgs();

  console.log('Publishing PAGX Viewer...');
  console.log(`Output: ${outputDir}\n`);

  // Build release (uses cache if available)
  console.log('Step 1: Build release...');
  runCommand('npm run build:release', VIEWER_DIR);

  // Copy index.html
  console.log('\nStep 2: Copy files...');
  copyFile(
    path.join(VIEWER_DIR, 'index.html'),
    path.join(outputDir, 'index.html')
  );

  // Copy index.css
  copyFile(
    path.join(VIEWER_DIR, 'index.css'),
    path.join(outputDir, 'index.css')
  );

  // Copy favicon and logo
  copyFile(
    path.join(VIEWER_DIR, 'favicon.png'),
    path.join(outputDir, 'favicon.png')
  );
  copyFile(
    path.join(VIEWER_DIR, 'logo.png'),
    path.join(outputDir, 'logo.png')
  );

  // Copy fonts from resources/font
  console.log('\n  Copying fonts...');
  copyFile(
    path.join(RESOURCES_FONT_DIR, 'NotoSansSC-Regular.otf'),
    path.join(outputDir, 'fonts', 'NotoSansSC-Regular.otf')
  );
  copyFile(
    path.join(RESOURCES_FONT_DIR, 'NotoColorEmoji.ttf'),
    path.join(outputDir, 'fonts', 'NotoColorEmoji.ttf')
  );

  // Copy wasm-mt directory
  console.log('\n  Copying wasm-mt...');
  const wasmDir = path.join(VIEWER_DIR, 'wasm-mt');
  const wasmOutputDir = path.join(outputDir, 'wasm-mt');
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
