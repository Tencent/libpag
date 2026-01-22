#!/usr/bin/env node

import fs from 'fs';
import path from 'path';

/**
 * Copy files from source directory to target directory based on filename patterns
 * @param {string} sourceDir - Source directory path
 * @param {string} targetDir - Target directory path
 * @param {string|Array} filenamePatterns - Filename patterns to match (supports wildcards and regex)
 * @param {Object} options - Additional options
 * @param {boolean} options.recursive - Whether to search subdirectories recursively
 * @param {boolean} options.overwrite - Whether to overwrite existing files
 * @param {boolean} options.preserveStructure - Whether to preserve directory structure
 */
function copyFiles(sourceDir, targetDir, filenamePatterns, options = {}) {
  const {
    recursive = false,
    overwrite = false,
    preserveStructure = false
  } = options;

  // Validate directories
  if (!fs.existsSync(sourceDir)) {
    throw new Error(`Source directory does not exist: ${sourceDir}`);
  }

  if (!fs.existsSync(targetDir)) {
    fs.mkdirSync(targetDir, { recursive: true });
  }

  // Convert single pattern to array
  const patterns = Array.isArray(filenamePatterns) ? filenamePatterns : [filenamePatterns];

  // Create regex patterns from wildcard patterns
  const regexPatterns = patterns.map(pattern => {
    // Convert wildcard pattern to regex
    const regexStr = pattern
      .replace(/\*/g, '.*')
      .replace(/\?/g, '.')
      .replace(/([.+^${}()|[\\]\])/g, '\\$1');
    return new RegExp(`^${regexStr}$`);
  });

  /**
   * Recursively search for files matching patterns
   */
  function searchFiles(dir, currentDepth = 0) {
    const files = [];

    try {
      const items = fs.readdirSync(dir);

      for (const item of items) {
        const fullPath = path.join(dir, item);
        const stat = fs.statSync(fullPath);

        if (stat.isDirectory()) {
          if (recursive) {
            files.push(...searchFiles(fullPath, currentDepth + 1));
          }
        } else if (stat.isFile()) {
          // Check if filename matches any pattern
          const matches = regexPatterns.some(regex => regex.test(item));
          if (matches) {
            files.push({
              sourcePath: fullPath,
              filename: item,
              relativePath: path.relative(sourceDir, fullPath)
            });
          }
        }
      }
    } catch (error) {
      console.error(`Error reading directory ${dir}:`, error.message);
    }

    return files;
  }

  // Find matching files
  const matchingFiles = searchFiles(sourceDir);

  if (matchingFiles.length === 0) {
    console.log('No files found matching the specified patterns.');
    return [];
  }

  console.log(`Found ${matchingFiles.length} file(s) to copy:`);

  const copiedFiles = [];

  // Copy files
  for (const file of matchingFiles) {
    try {
      let targetPath;

      if (preserveStructure && file.relativePath !== file.filename) {
        // Preserve directory structure
        const targetSubDir = path.dirname(file.relativePath);
        const fullTargetDir = path.join(targetDir, targetSubDir);

        if (!fs.existsSync(fullTargetDir)) {
          fs.mkdirSync(fullTargetDir, { recursive: true });
        }

        targetPath = path.join(fullTargetDir, file.filename);
      } else {
        // Copy to root of target directory
        targetPath = path.join(targetDir, file.filename);
      }

      // Check if target file exists
      if (fs.existsSync(targetPath)) {
        if (!overwrite) {
          console.log(`Skipping ${file.filename} (already exists)`);
          continue;
        }
        console.log(`Overwriting ${file.filename}`);
      } else {
        console.log(`Copying ${file.filename}`);
      }

      // Copy file
      fs.copyFileSync(file.sourcePath, targetPath);
      copiedFiles.push({
        source: file.sourcePath,
        target: targetPath,
        filename: file.filename
      });

    } catch (error) {
      console.error(`Error copying ${file.filename}:`, error.message);
    }
  }

  console.log(`Successfully copied ${copiedFiles.length} file(s)`);
  return copiedFiles;
}

/**
 * Command line interface
 */
function main() {
  const args = process.argv.slice(2);

  if (args.length < 3) {
    console.log('Usage: node copy-files.js <source-dir> <target-dir> <filename-pattern> [options]');
    console.log('');
    console.log('Examples:');
    console.log('  node copy-files.js ./src ./dist "*.js"');
    console.log('  node copy-files.js ./src ./dist "*.ts" --recursive --overwrite');
    console.log('  node copy-files.js ./src ./dist "*.js;*.ts" --preserve-structure');
    console.log('');
    console.log('Options:');
    console.log('  --recursive          Search subdirectories recursively');
    console.log('  --overwrite          Overwrite existing files');
    console.log('  --preserve-structure Preserve directory structure');
    return;
  }

  const sourceDir = args[0];
  const targetDir = args[1];
  const filenamePatterns = args[2].split(';'); // Support multiple patterns separated by semicolon

  const options = {
    recursive: args.includes('--recursive'),
    overwrite: args.includes('--overwrite'),
    preserveStructure: args.includes('--preserve-structure')
  };

  try {
    copyFiles(sourceDir, targetDir, filenamePatterns, options);
  } catch (error) {
    console.error('Error:', error.message);
    process.exit(1);
  }
}

// Export for module usage
// if (require.main === module) {
//     main();
// } else {
//     module.exports = { copyFiles };
// }

copyFiles('/Users/billyjin/Desktop/project/tgfx_new/pagx/wechat/ts/wasm', '/Users/billyjin/Desktop/project/tgfx_new/pagx/wechat/wx_demo/utils',
  ['*.js', '*.br'], {
  recursive: true,
  overwrite: true,
  preserveStructure: true
});