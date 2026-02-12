#!/usr/bin/env node
/**
 * PAGX Playground Publisher
 *
 * Builds and copies the PAGX Playground to the public directory.
 *
 * Source files:
 *     index.html
 *     index.css
 *     wasm-mt/
 *     ../../resources/font/  (from libpag root)
 *     ../../spec/samples/    (from libpag root)
 *
 * Output structure:
 *     <output>/index.html
 *     <output>/index.css
 *     <output>/fonts/
 *     <output>/wasm-mt/
 *     <output>/samples/          (.pagx files, images, and generated index.json)
 *
 * Usage:
 *     npm run publish [-- -o <output-dir>]
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// Default paths
const SCRIPT_DIR = __dirname;
const PLAYGROUND_DIR = path.dirname(SCRIPT_DIR);
const LIBPAG_DIR = path.dirname(PLAYGROUND_DIR);
const RESOURCES_FONT_DIR = path.join(LIBPAG_DIR, 'resources', 'font');
const SAMPLES_DIR = path.join(LIBPAG_DIR, 'spec', 'samples');
const DEFAULT_OUTPUT_DIR = path.join(LIBPAG_DIR, 'public');

/**
 * Parse command line arguments.
 */
function parseArgs() {
  const args = process.argv.slice(2);
  let outputDir = DEFAULT_OUTPUT_DIR;
  let skipBuild = false;

  for (let i = 0; i < args.length; i++) {
    if ((args[i] === '-o' || args[i] === '--output') && args[i + 1]) {
      outputDir = path.resolve(args[i + 1]);
      i++;
    } else if (args[i] === '--skip-build') {
      skipBuild = true;
    } else if (args[i] === '-h' || args[i] === '--help') {
      console.log(`
PAGX Playground Publisher

Usage:
    npm run publish [-- -o <output-dir>] [-- --skip-build]

Options:
    -o, --output <dir>  Output directory (default: ../public)
    --skip-build        Skip build step (use pre-built wasm-mt directory)
    -h, --help          Show this help message
`);
      process.exit(0);
    }
  }

  return { outputDir, skipBuild };
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
 * Run a command with timeout and improved error handling.
 */
function runCommand(command, cwd, timeout = 600000) {
  console.log(`  Running: ${command}`);
  try {
    execSync(command, { 
      cwd, 
      stdio: 'inherit',
      timeout: timeout  // 10 minutes default
    });
  } catch (error) {
    if (error.killed) {
      console.error(`  ERROR: Command timed out after ${timeout/1000} seconds`);
      console.error(`  If build is taking longer, run directly:`);
      console.error(`    cd ${cwd}`);
      console.error(`    npm run build:release`);
    } else {
      console.error(`  ERROR: Command failed with exit code ${error.status}`);
    }
    process.exit(1);
  }
}

/**
 * Main function.
 */
function main() {
  const { outputDir, skipBuild } = parseArgs();

  console.log('Publishing PAGX Playground...');
  console.log(`Output: ${outputDir}\n`);

  // Build release (uses cache if available)
  if (!skipBuild) {
    console.log('Step 1: Build release...');
    runCommand('npm run build:release', PLAYGROUND_DIR, 600000);  // 10 minute timeout
  } else {
    console.log('Step 1: Skipping build (--skip-build flag set)');
    if (!fs.existsSync(path.join(PLAYGROUND_DIR, 'wasm-mt', 'pagx-playground.wasm'))) {
      console.error('ERROR: wasm-mt/pagx-playground.wasm not found. Run build first or remove --skip-build flag');
      process.exit(1);
    }
  }

  // Copy index.html
  console.log('\nStep 2: Copy files...');
  copyFile(
    path.join(PLAYGROUND_DIR, 'index.html'),
    path.join(outputDir, 'index.html')
  );

  // Copy index.css
  copyFile(
    path.join(PLAYGROUND_DIR, 'index.css'),
    path.join(outputDir, 'index.css')
  );

  // Copy favicon and logo
  copyFile(
    path.join(PLAYGROUND_DIR, 'favicon.png'),
    path.join(outputDir, 'favicon.png')
  );
  copyFile(
    path.join(PLAYGROUND_DIR, 'logo.png'),
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
  const wasmDir = path.join(PLAYGROUND_DIR, 'wasm-mt');
  const wasmOutputDir = path.join(outputDir, 'wasm-mt');
  copyFile(
    path.join(wasmDir, 'index.js'),
    path.join(wasmOutputDir, 'index.js')
  );
  copyFile(
    path.join(wasmDir, 'pagx-playground.js'),
    path.join(wasmOutputDir, 'pagx-playground.js')
  );
  copyFile(
    path.join(wasmDir, 'pagx-playground.wasm'),
    path.join(wasmOutputDir, 'pagx-playground.wasm')
  );

  // Copy samples directory and generate index.json
  console.log('\n  Copying samples...');
  const samplesOutputDir = path.join(outputDir, 'samples');
  const sampleFiles = fs.readdirSync(SAMPLES_DIR)
    .filter(f => !f.startsWith('.'))
    .sort();
  for (const file of sampleFiles) {
    copyFile(
      path.join(SAMPLES_DIR, file),
      path.join(samplesOutputDir, file)
    );
  }
  const pagxFiles = sampleFiles.filter(f => f.endsWith('.pagx'));
  const indexJsonPath = path.join(samplesOutputDir, 'index.json');
  fs.mkdirSync(path.dirname(indexJsonPath), { recursive: true });
  fs.writeFileSync(indexJsonPath, JSON.stringify(pagxFiles, null, 2) + '\n');
  console.log(`  Generated: ${indexJsonPath}`);

  console.log('\nDone!');
}

main();
