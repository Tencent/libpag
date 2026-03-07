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
 *     ../../resources/font/       (from libpag root)
 *     ../../spec/samples/         (from libpag root)
 *     ../../.codebuddy/skills/    (from libpag root)
 *
 * Output structure:
 *     <output>/index.html
 *     <output>/index.css
 *     <output>/fonts/
 *     <output>/wasm-mt/
 *     <output>/samples/           (.pagx files, images, and generated index.json)
 *     <output>/skills/<name>/     (static skill files for online browsing)
 *     <output>/skills/<name>.zip  (skill archive for AI tool downloads)
 *     <output>/skills/index.html  (skills documentation page - English)
 *     <output>/skills/zh/index.html (skills documentation page - Chinese)
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
const SKILLS_DIR = path.join(LIBPAG_DIR, '.codebuddy', 'skills');
const DEFAULT_OUTPUT_DIR = path.join(LIBPAG_DIR, 'public');

/**
 * Format date in English (e.g., "6 March 2026").
 */
function formatDateEn(date) {
  const months = ['January', 'February', 'March', 'April', 'May', 'June',
                  'July', 'August', 'September', 'October', 'November', 'December'];
  return date.getDate() + ' ' + months[date.getMonth()] + ' ' + date.getFullYear();
}

/**
 * Format date in Chinese (e.g., "2026 \u5e74 3 \u6708 6 \u65e5").
 */
function formatDateZh(date) {
  return date.getFullYear() + ' \u5e74 ' + (date.getMonth() + 1) + ' \u6708 ' + date.getDate() + ' \u65e5';
}

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
 * Recursively copy a directory.
 */
function copyDir(src, dest) {
  fs.mkdirSync(dest, { recursive: true });
  for (const entry of fs.readdirSync(src, { withFileTypes: true })) {
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);
    if (entry.isDirectory()) {
      copyDir(srcPath, destPath);
    } else {
      copyFile(srcPath, destPath);
    }
  }
}

/**
 * Check whether two directories have identical file content recursively.
 * Returns true if every file exists in both directories with the same content.
 */
function dirsEqual(dirA, dirB) {
  if (!fs.existsSync(dirA) || !fs.existsSync(dirB)) {
    return false;
  }
  const entriesA = fs.readdirSync(dirA, { withFileTypes: true }).filter(e => !e.name.startsWith('.'));
  const entriesB = fs.readdirSync(dirB, { withFileTypes: true }).filter(e => !e.name.startsWith('.'));
  if (entriesA.length !== entriesB.length) {
    return false;
  }
  const namesB = new Set(entriesB.map(e => e.name));
  for (const entry of entriesA) {
    if (!namesB.has(entry.name)) {
      return false;
    }
    const pathA = path.join(dirA, entry.name);
    const pathB = path.join(dirB, entry.name);
    if (entry.isDirectory()) {
      if (!dirsEqual(pathA, pathB)) {
        return false;
      }
    } else {
      if (!fs.readFileSync(pathA).equals(fs.readFileSync(pathB))) {
        return false;
      }
    }
  }
  return true;
}

/**
 * Extract the last-updated date string from an existing skills HTML file.
 * Returns the matched date string, or null if not found.
 */
function extractDate(htmlPath) {
  if (!fs.existsSync(htmlPath)) {
    return null;
  }
  const html = fs.readFileSync(htmlPath, 'utf-8');
  var match = html.match(/Last updated: (.+?)</) || html.match(/最后更新：(.+?)</);
  return match ? match[1] : null;
}

/**
 * Publish skills as static files, zip archives, and documentation pages.
 * Each named skill is copied to <outputDir>/skills/<name>/ and packaged as
 * <outputDir>/skills/<name>.zip (without the outer directory).
 * Additionally generates skills/index.html (English) and skills/zh/index.html (Chinese).
 */
function publishSkills(outputDir, names) {
  const skillsOutputDir = path.join(outputDir, 'skills');
  let skillsChanged = false;

  for (const name of names) {
    const skillSrcDir = path.join(SKILLS_DIR, name);
    if (!fs.existsSync(skillSrcDir)) {
      console.warn(`  Warning: skill directory not found: ${skillSrcDir}`);
      continue;
    }

    const skillDestDir = path.join(skillsOutputDir, name);
    const zipPath = path.join(skillsOutputDir, `${name}.zip`);

    // Compare source and cached destination to decide whether to rebuild
    if (dirsEqual(skillSrcDir, skillDestDir) && fs.existsSync(zipPath)) {
      console.log(`  Unchanged: ${skillDestDir}`);
      console.log(`  Unchanged: ${zipPath}`);
      continue;
    }

    skillsChanged = true;

    // Copy static files
    if (fs.existsSync(skillDestDir)) {
      fs.rmSync(skillDestDir, { recursive: true });
    }
    copyDir(skillSrcDir, skillDestDir);

    // Create zip (contents only, no outer directory)
    fs.mkdirSync(skillsOutputDir, { recursive: true });
    if (fs.existsSync(zipPath)) {
      fs.unlinkSync(zipPath);
    }
    execSync(`zip -r "${zipPath}" .`, { cwd: skillSrcDir, stdio: 'pipe' });
    console.log(`  Created: ${zipPath}`);
  }

  // Determine the timestamp for skills pages.
  // If skills content is unchanged, preserve the existing date; otherwise use today.
  const enOutput = path.join(skillsOutputDir, 'index.html');
  const zhOutput = path.join(skillsOutputDir, 'zh', 'index.html');
  var dateEn;
  var dateZh;
  if (skillsChanged) {
    const now = new Date();
    dateEn = formatDateEn(now);
    dateZh = formatDateZh(now);
  } else {
    dateEn = extractDate(enOutput) || formatDateEn(new Date());
    dateZh = extractDate(zhOutput) || formatDateZh(new Date());
  }

  // Always publish skills documentation pages
  const enTemplate = fs.readFileSync(
    path.join(PLAYGROUND_DIR, 'skills-page.html'), 'utf-8'
  );
  fs.mkdirSync(path.dirname(enOutput), { recursive: true });
  fs.writeFileSync(enOutput, enTemplate.replace('{{lastUpdated}}', dateEn), 'utf-8');
  console.log(`  Published: ${enOutput}`);

  const zhTemplate = fs.readFileSync(
    path.join(PLAYGROUND_DIR, 'skills-page.zh_CN.html'), 'utf-8'
  );
  fs.mkdirSync(path.dirname(zhOutput), { recursive: true });
  fs.writeFileSync(zhOutput, zhTemplate.replace('{{lastUpdated}}', dateZh), 'utf-8');
  console.log(`  Published: ${zhOutput}`);
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
  if (fs.existsSync(samplesOutputDir)) {
    fs.rmSync(samplesOutputDir, { recursive: true });
  }
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

  // Copy baseline images that match sample pagx files
  console.log('\n  Copying sample images...');
  const testOutputDir = path.join(LIBPAG_DIR, 'test', 'out', 'PAGXTest');
  if (fs.existsSync(testOutputDir)) {
    const imagesOutputDir = path.join(samplesOutputDir, 'images');
    fs.mkdirSync(imagesOutputDir, { recursive: true });

    for (const pagxFile of pagxFiles) {
      const baseName = pagxFile.replace(/\.pagx$/, '');
      const baselineFile = baseName + '_base.webp';
      const src = path.join(testOutputDir, baselineFile);
      if (fs.existsSync(src)) {
        copyFile(src, path.join(imagesOutputDir, baseName + '.webp'));
      }
    }
  } else {
    console.warn('  Warning: test output directory not found at', testOutputDir);
  }

  // Publish skills
  console.log('\n  Publishing skills...');
  publishSkills(outputDir, ['pagx']);

  console.log('\nDone!');
}

main();
