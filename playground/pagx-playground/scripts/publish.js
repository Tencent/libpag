#!/usr/bin/env node
/**
 * PAGX Playground & Spec Publisher
 *
 * Builds and copies the PAGX Playground to the public directory, publishes
 * skills documentation, and generates the PAGX specification HTML pages.
 *
 * Source files:
 *     index.html
 *     index.css
 *     wasm-mt/
 *     ../../resources/font/       (from libpag root)
 *     ../../spec/samples/         (from libpag root)
 *     ../../.codebuddy/skills/    (from libpag root)
 *     ../../spec/pagx_spec.md     (from libpag root)
 *     ../../spec/pagx_spec.zh_CN.md
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
 *     <output>/<version>/index.html (spec - English)
 *     <output>/<version>/zh/index.html (spec - Chinese)
 *     <output>/latest/            (spec - latest version copy)
 *
 * Usage:
 *     npm run publish [-- -o <output-dir>] [-- --skip-build]
 */

import fs from 'fs';
import path from 'path';
import { execSync } from 'child_process';
import { fileURLToPath } from 'url';
import { Marked } from 'marked';
import { markedHighlight } from 'marked-highlight';
import { gfmHeadingId } from 'marked-gfm-heading-id';
import hljs from 'highlight.js';

// ESM equivalent of __dirname
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Default paths
const SCRIPT_DIR = __dirname;
const PLAYGROUND_DIR = path.dirname(SCRIPT_DIR);
// PLAYGROUND_DIR is libpag/playground/pagx-playground, so go up two levels to get libpag root
const LIBPAG_DIR = path.dirname(path.dirname(PLAYGROUND_DIR));
const RESOURCES_FONT_DIR = path.join(LIBPAG_DIR, 'resources', 'font');
const SAMPLES_DIR = path.join(LIBPAG_DIR, 'spec', 'samples');
const SKILLS_DIR = path.join(LIBPAG_DIR, '.codebuddy', 'skills');
const DEFAULT_OUTPUT_DIR = path.join(LIBPAG_DIR, 'public');

// Spec paths
const SPEC_DIR = path.join(LIBPAG_DIR, 'spec');
const SPEC_FILE_EN = path.join(SPEC_DIR, 'pagx_spec.md');
const SPEC_FILE_ZH = path.join(SPEC_DIR, 'pagx_spec.zh_CN.md');
const PAGES_DIR = path.join(PLAYGROUND_DIR, 'pages');
const SPEC_TEMPLATE_FILE = path.join(PAGES_DIR, 'spec', 'template.html');
const PACKAGE_FILE = path.join(PLAYGROUND_DIR, 'package.json');

// Base URL for the spec site
const BASE_URL = 'https://pag.io/pagx';

// ============================================================================
// Common utilities
// ============================================================================

/**
 * Format date in English (e.g., "6 March 2026").
 */
function formatDateEn(date) {
  const months = ['January', 'February', 'March', 'April', 'May', 'June',
                  'July', 'August', 'September', 'October', 'November', 'December'];
  return date.getDate() + ' ' + months[date.getMonth()] + ' ' + date.getFullYear();
}

/**
 * Format date in Chinese (e.g., "2026 年 3 月 6 日").
 */
function formatDateZh(date) {
  return date.getFullYear() + ' 年 ' + (date.getMonth() + 1) + ' 月 ' + date.getDate() + ' 日';
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
PAGX Playground & Spec Publisher

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
 * Check whether two files have identical content.
 */
function filesEqual(fileA, fileB) {
  if (!fs.existsSync(fileA) || !fs.existsSync(fileB)) {
    return false;
  }
  return fs.readFileSync(fileA).equals(fs.readFileSync(fileB));
}

/**
 * Extract the last-updated date string from an existing HTML file.
 */
function extractDate(htmlPath) {
  if (!fs.existsSync(htmlPath)) {
    return null;
  }
  const html = fs.readFileSync(htmlPath, 'utf-8');
  const match = html.match(/Last updated:.*?(\d{1,2} \w+ \d{4})/) || html.match(/最后更新：.*?(\d{4} 年 \d{1,2} 月 \d{1,2} 日)/);
  return match ? match[1].trim() : null;
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
      timeout: timeout
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

// ============================================================================
// Skills publishing
// ============================================================================

/**
 * Publish skills as static files, zip archives, and documentation pages.
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

    if (dirsEqual(skillSrcDir, skillDestDir) && fs.existsSync(zipPath)) {
      console.log(`  Unchanged: ${skillDestDir}`);
      console.log(`  Unchanged: ${zipPath}`);
      continue;
    }

    skillsChanged = true;

    if (fs.existsSync(skillDestDir)) {
      fs.rmSync(skillDestDir, { recursive: true });
    }
    copyDir(skillSrcDir, skillDestDir);

    fs.mkdirSync(skillsOutputDir, { recursive: true });
    if (fs.existsSync(zipPath)) {
      fs.unlinkSync(zipPath);
    }
    execSync(`zip -r "${zipPath}" .`, { cwd: skillDestDir, stdio: 'pipe' });
    console.log(`  Created: ${zipPath}`);
  }

  const enOutput = path.join(skillsOutputDir, 'index.html');
  const zhOutput = path.join(skillsOutputDir, 'zh', 'index.html');
  let dateEn;
  let dateZh;
  if (skillsChanged) {
    const now = new Date();
    dateEn = formatDateEn(now);
    dateZh = formatDateZh(now);
  } else {
    dateEn = extractDate(enOutput) || formatDateEn(new Date());
    dateZh = extractDate(zhOutput) || formatDateZh(new Date());
  }

  const enTemplate = fs.readFileSync(
    path.join(PAGES_DIR, 'skills', 'index.html'), 'utf-8'
  );
  fs.mkdirSync(path.dirname(enOutput), { recursive: true });
  fs.writeFileSync(enOutput, enTemplate.replace('{{lastUpdated}}', dateEn), 'utf-8');
  console.log(`  Published: ${enOutput}`);

  const zhTemplate = fs.readFileSync(
    path.join(PAGES_DIR, 'skills', 'zh', 'index.html'), 'utf-8'
  );
  fs.mkdirSync(path.dirname(zhOutput), { recursive: true });
  fs.writeFileSync(zhOutput, zhTemplate.replace('{{lastUpdated}}', dateZh), 'utf-8');
  console.log(`  Published: ${zhOutput}`);
}

// ============================================================================
// Spec publishing
// ============================================================================

/**
 * Read spec version from package.json.
 */
function getSpecVersion() {
  const pkg = JSON.parse(fs.readFileSync(PACKAGE_FILE, 'utf-8'));
  return pkg.specVersion;
}

/**
 * Read spec stable version from package.json.
 */
function getSpecStableVersion() {
  const pkg = JSON.parse(fs.readFileSync(PACKAGE_FILE, 'utf-8'));
  return pkg.specStableVersion || '';
}

/**
 * Convert heading text to URL-friendly slug.
 */
function slugify(text) {
  return text.toLowerCase()
    .replace(/^[\d.]+\s+/, '')
    .replace(/[^\w\s-]/g, '')
    .replace(/[\s_]+/g, '-')
    .replace(/-+/g, '-')
    .replace(/^-|-$/g, '') || 'section';
}

/**
 * Extract all heading slugs from English Markdown content.
 */
function extractEnglishSlugs(content) {
  const slugs = [];
  const slugCounts = {};
  let inAppendix = false;

  for (const line of content.split('\n')) {
    const match = line.match(/^(#{1,6})\s+(.+)$/);
    if (!match) continue;

    const text = match[2].trim();
    if (text.startsWith('Appendix')) inAppendix = true;

    let slug = slugify(text);
    if (inAppendix && !text.startsWith('Appendix')) {
      slug = 'ref-' + slug;
    }

    if (slugCounts[slug] !== undefined) {
      slugCounts[slug]++;
      slug = `${slug}-${slugCounts[slug]}`;
    } else {
      slugCounts[slug] = 0;
    }

    slugs.push(slug);
  }
  return slugs;
}

/**
 * Extract headings from Markdown for TOC generation.
 */
function parseMarkdownHeadings(content, englishSlugs = null) {
  const headings = [];
  const slugCounts = {};
  let slugIndex = 0;

  for (const line of content.split('\n')) {
    const match = line.match(/^(#{1,6})\s+(.+)$/);
    if (!match) continue;

    const level = match[1].length;
    const text = match[2].trim();
    let slug;

    if (englishSlugs && slugIndex < englishSlugs.length) {
      slug = englishSlugs[slugIndex++];
    } else {
      slug = slugify(text);
      if (slugCounts[slug] !== undefined) {
        slugCounts[slug]++;
        slug = `${slug}-${slugCounts[slug]}`;
      } else {
        slugCounts[slug] = 0;
      }
    }

    headings.push({ level, text, slug });
  }
  return headings;
}

/**
 * Generate HTML for table of contents.
 */
function generateTocHtml(headings) {
  if (!headings.length) return '';

  const html = ['<ul>'];
  const stack = [1];

  for (let i = 0; i < headings.length; i++) {
    const { level, text, slug } = headings[i];
    if (level === 1) continue;

    const adjustedLevel = level - 1;

    while (stack.length > adjustedLevel) {
      html.push('</ul></li>');
      stack.pop();
    }
    while (stack.length < adjustedLevel) {
      html.push('<li><ul>');
      stack.push(stack.length + 1);
    }

    html.push(`<li><a href="#${slug}">${text}</a>`);

    if (i + 1 < headings.length && headings[i + 1].level > level) {
      html.push('<ul>');
      stack.push(stack.length + 1);
    } else {
      html.push('</li>');
    }
  }

  while (stack.length > 1) {
    html.push('</ul></li>');
    stack.pop();
  }

  html.push('</ul>');
  return html.join('\n');
}

/**
 * Create configured Marked instance.
 */
function createMarkedInstance() {
  const slugCounts = {};

  return new Marked(
    markedHighlight({
      langPrefix: 'hljs language-',
      highlight(code, lang) {
        if (lang && hljs.getLanguage(lang)) {
          return hljs.highlight(code, { language: lang }).value;
        }
        return hljs.highlightAuto(code).value;
      },
    }),
    gfmHeadingId({
      prefix: '',
      slug: (text) => {
        let slug = slugify(text);
        if (slugCounts[slug] !== undefined) {
          slugCounts[slug]++;
          slug = `${slug}-${slugCounts[slug]}`;
        } else {
          slugCounts[slug] = 0;
        }
        return slug;
      },
    })
  );
}

/**
 * Replace heading IDs in HTML content with provided slugs.
 */
function replaceHeadingIds(html, slugs) {
  let i = 0;
  return html.replace(/<h([1-6])\s+id="[^"]*"/g, (match, level) => {
    return i < slugs.length ? `<h${level} id="${slugs[i++]}"` : match;
  });
}

/**
 * Generate HTML document from spec template.
 */
function generateSpecHtml(content, title, tocHtml, lang, langSwitchUrl, viewerUrl, faviconUrl) {
  const template = fs.readFileSync(SPEC_TEMPLATE_FILE, 'utf-8');
  const isEnglish = lang === 'en';

  const vars = {
    htmlLang: isEnglish ? 'en' : 'zh-CN',
    title,
    faviconUrl,
    viewerUrl,
    tocTitle: isEnglish ? 'Table of Contents' : '目录',
    tocLabel: isEnglish ? 'TOC' : '目录',
    previewLabel: isEnglish ? 'Preview' : '在线预览',
    currentLang: isEnglish ? 'English' : '简体中文',
    enUrl: isEnglish ? '#' : langSwitchUrl,
    zhUrl: isEnglish ? langSwitchUrl : '#',
    enActive: isEnglish ? ' class="active"' : '',
    zhActive: isEnglish ? '' : ' class="active"',
    zhRedirectUrl: isEnglish ? langSwitchUrl : '',
    tocHtml,
    content,
  };

  return template.replace(/\{\{(\w+)\}\}/g, (_, key) => vars[key] || '');
}

/**
 * Embed sample file contents into Markdown content.
 */
function embedSampleFiles(mdContent, specDir) {
  const samplePattern = /^> \[Sample\]\((samples\/[^\s)]+\.pagx)\)$/gm;
  return mdContent.replace(samplePattern, (match, samplePath) => {
    const filePath = path.join(specDir, samplePath);
    if (!fs.existsSync(filePath)) {
      console.warn(`  Warning: sample file not found: ${filePath}`);
      return match;
    }
    const content = fs.readFileSync(filePath, 'utf-8').trimEnd();
    return '```xml\n' + content + '\n```\n\n<!-- preview:' + samplePath + ' -->';
  });
}

/**
 * Post-process HTML to add preview headers to code blocks.
 */
function addPreviewButtons(html, viewerUrl, lang) {
  const markerPattern = /<!-- preview:(samples\/[^\s]+\.pagx) -->/g;
  const markers = [];
  let m;
  while ((m = markerPattern.exec(html)) !== null) {
    markers.push({ start: m.index, end: m.index + m[0].length, samplePath: m[1] });
  }
  if (markers.length === 0) return html;
  for (let i = markers.length - 1; i >= 0; i--) {
    const marker = markers[i];
    const preCloseTag = '</pre>';
    let preCloseEnd = html.lastIndexOf(preCloseTag, marker.start);
    if (preCloseEnd === -1) {
      console.warn('  Warning: no </pre> found before preview marker for ' + marker.samplePath);
      continue;
    }
    preCloseEnd += preCloseTag.length;
    const preOpenStart = html.lastIndexOf('<pre>', preCloseEnd);
    if (preOpenStart === -1) {
      console.warn('  Warning: no <pre> found before preview marker for ' + marker.samplePath);
      continue;
    }
    const sampleName = path.basename(marker.samplePath);
    const previewUrl = viewerUrl + '?sample=' + sampleName;
    const label = lang === 'zh' ? '预览' : 'Preview';
    const header = '<div class="code-header">' +
      '<a class="preview-btn" href="' + previewUrl + '">' +
      '<svg viewBox="0 0 24 24" fill="currentColor"><polygon points="5 3 19 12 5 21 5 3"></polygon></svg>' +
      label + '</a>' +
      '<span class="code-header-label">PAGX</span></div>';
    const wrapperOpen = '<div class="code-block-wrapper">' + header;
    const wrapperClose = '</div>';
    html = html.slice(0, preCloseEnd) + wrapperClose + html.slice(marker.end);
    html = html.slice(0, preOpenStart) + wrapperOpen + html.slice(preOpenStart);
  }
  return html;
}

/**
 * Publish a single spec file to HTML.
 */
function publishSpecFile(specFile, outputDir, lang, langSwitchUrl, viewerUrl, faviconUrl, englishSlugs = null) {
  if (!fs.existsSync(specFile)) {
    console.log(`  Skipped (file not found): ${specFile}`);
    return;
  }

  console.log(`  Reading: ${specFile}`);
  let mdContent = fs.readFileSync(specFile, 'utf-8');
  mdContent = embedSampleFiles(mdContent, SPEC_DIR);

  const titleMatch = mdContent.match(/^#\s+(.+)$/m);
  const title = titleMatch ? titleMatch[1] : 'PAGX Format Specification';

  const headings = parseMarkdownHeadings(mdContent, englishSlugs);
  const tocHtml = generateTocHtml(headings);

  const marked = createMarkedInstance();
  let htmlContent = marked.parse(mdContent);

  if (englishSlugs) {
    htmlContent = replaceHeadingIds(htmlContent, englishSlugs);
  }

  htmlContent = addPreviewButtons(htmlContent, viewerUrl, lang);

  const html = generateSpecHtml(htmlContent, title, tocHtml, lang, langSwitchUrl, viewerUrl, faviconUrl);

  fs.mkdirSync(outputDir, { recursive: true });
  const outputFile = path.join(outputDir, 'index.html');
  fs.writeFileSync(outputFile, html, 'utf-8');
  console.log(`  Published: ${outputFile}`);
}

/**
 * Find all published spec versions in the site directory.
 */
function findPublishedVersions(siteDir) {
  if (!fs.existsSync(siteDir)) return [];

  const entries = fs.readdirSync(siteDir, { withFileTypes: true });
  const versions = [];

  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    if (['latest', 'fonts', 'wasm-mt', 'node_modules', 'viewer', 'samples', 'skills'].includes(entry.name)) continue;
    if (!/^\d+\.\d+(\.\d+)?$/.test(entry.name)) continue;
    const indexPath = path.join(siteDir, entry.name, 'index.html');
    if (fs.existsSync(indexPath)) {
      versions.push(entry.name);
    }
  }

  return versions;
}

/**
 * Generate version info block HTML.
 */
function generateVersionInfoHtml(thisVersion, draftVersion, stableVersion, isZh, lastUpdated) {
  const langSuffix = isZh ? 'zh/' : '';
  const thisUrl = `${BASE_URL}/${thisVersion}/${langSuffix}`;
  const latestUrl = isZh ? `${BASE_URL}/latest/zh/` : `${BASE_URL}/latest/`;

  const isOutdated = stableVersion && thisVersion !== stableVersion &&
    thisVersion.localeCompare(stableVersion, undefined, { numeric: true }) < 0;
  const isDraft = !isOutdated && thisVersion !== stableVersion;

  let statusClass, statusText;
  if (isOutdated) {
    statusClass = 'outdated';
    statusText = isZh
      ? `版本 ${thisVersion} (过时) — 当前版本不是最新版本，请查阅最新版本获取最新内容。`
      : `Version ${thisVersion} (Outdated) — This is not the latest version. Please refer to the latest version for up-to-date content.`;
  } else if (isDraft) {
    statusClass = 'draft';
    statusText = isZh
      ? `版本 ${thisVersion} (草稿) — 本规范为草稿版本，内容可能随时变更。`
      : `Version ${thisVersion} (Draft) — This specification is a working draft and may change at any time.`;
  } else {
    statusClass = 'stable';
    statusText = isZh ? `版本 ${thisVersion} (正式)` : `Version ${thisVersion} (Stable)`;
  }

  const labels = isZh
    ? { updated: '最后更新：', this: '当前版本：', latest: '最新版本：', draft: '草稿版本：' }
    : { updated: 'Last updated:', this: 'This version:', latest: 'Latest version:', draft: "Editor's draft:" };

  let linksHtml = `<p><strong>${labels.updated}</strong> ${lastUpdated}</p>
<p><strong>${labels.this}</strong> <a href="${thisUrl}">${thisUrl}</a></p>
<p><strong>${labels.latest}</strong> <a href="${latestUrl}">${latestUrl}</a></p>`;

  if (draftVersion !== stableVersion) {
    const draftUrl = `${BASE_URL}/${draftVersion}/${langSuffix}`;
    linksHtml += `\n<p><strong>${labels.draft}</strong> <a href="${draftUrl}">${draftUrl}</a></p>`;
  }

  return `<div class="version-info ${statusClass}">
<div class="version-status">${statusText}</div>
${linksHtml}
</div>`;
}

/**
 * Update version links in an HTML file. Preserves the original "last updated"
 * time from the existing HTML; falls back to the provided date if not found.
 */
function updateVersionLinks(htmlFile, thisVersion, draftVersion, stableVersion, isZh, fallbackDate) {
  if (!fs.existsSync(htmlFile)) return false;

  let html = fs.readFileSync(htmlFile, 'utf-8');

  const dateMatch = html.match(/Last updated:.*?(\d{1,2} \w+ \d{4})/) || html.match(/最后更新：.*?(\d{4} 年 \d{1,2} 月 \d{1,2} 日)/);
  const lastUpdated = (dateMatch ? dateMatch[1].trim() : null) || fallbackDate;
  const versionInfoHtml = generateVersionInfoHtml(thisVersion, draftVersion, stableVersion, isZh, lastUpdated);

  const versionInfoCss = `/* Version info block */
        .version-info {
            border-left: 4px solid;
            padding: 12px 16px;
            margin-bottom: 24px;
        }
        .version-info.stable {
            border-color: #1e7e34;
            background-color: #f0f9f1;
        }
        .version-info.draft {
            border-color: #d4a000;
            background-color: #fffbf0;
        }
        .version-info.outdated {
            border-color: #c42c2c;
            background-color: #fef6f6;
        }
        .version-info .version-status {
            font-weight: 600;
            margin-bottom: 8px;
        }
        .version-info.stable .version-status { color: #1e5631; }
        .version-info.draft .version-status { color: #6a4c00; }
        .version-info.outdated .version-status { color: #82071e; }
        .version-info p { margin-bottom: 4px; font-size: 14px; color: #555; }`;

  if (html.includes('/* Version info block */')) {
    html = html.replace(
      /\/\* Version info block \*\/[\s\S]*?\.version-info p \{[^}]+\}/,
      versionInfoCss
    );
  } else {
    html = html.replace(
      /(img \{[^}]+\})/,
      `$1\n\n        ${versionInfoCss}`
    );
  }

  html = html.replace(
    /h1 \{ font-size: 2em; margin-bottom: 16px; \}/,
    'h1 { font-size: 2em; padding-bottom: 0.3em; border-bottom: 1px solid var(--border-color); margin-bottom: 16px; }'
  );

  const versionBlockRegex = /(<h1[^>]*>)[^<]*(<\/h1>)[\s\S]*?(<h2)/;

  const match = html.match(versionBlockRegex);
  if (match) {
    const h1Open = match[1];
    const h1Close = match[2];
    const h2Open = match[3];

    const titleMatch = html.match(/<h1[^>]*>([^<]*)<\/h1>/);
    let title = titleMatch ? titleMatch[1] : '';
    title = title.replace(/\s+\d+\.\d+(\.\d+)?$/, '');

    const cleanH1Open = h1Open.replace(/id="[^"]*"/, 'id="pagx-format-specification"');

    const newContent = `${cleanH1Open}${title}${h1Close}\n${versionInfoHtml}\n${h2Open}`;
    html = html.replace(versionBlockRegex, newContent);
    fs.writeFileSync(htmlFile, html, 'utf-8');
    return true;
  }
  return false;
}

/**
 * Update version links in all published versions. Each version's HTML preserves
 * its own "last updated" date; fallback dates are used only when not found.
 */
function updateAllVersionLinks(siteDir, draftVersion, stableVersion, fallbackDateEn, fallbackDateZh) {
  const versions = findPublishedVersions(siteDir);
  if (versions.length === 0) return;

  console.log('\n  Updating version links in all versions...');

  for (const ver of versions) {
    const enFile = path.join(siteDir, ver, 'index.html');
    const zhFile = path.join(siteDir, ver, 'zh', 'index.html');

    if (updateVersionLinks(enFile, ver, draftVersion, stableVersion, false, fallbackDateEn)) {
      console.log(`  Updated: ${enFile}`);
    }
    if (updateVersionLinks(zhFile, ver, draftVersion, stableVersion, true, fallbackDateZh)) {
      console.log(`  Updated: ${zhFile}`);
    }
  }
}

/**
 * Copy latest version files to latest folder, including markdown source copies
 * for change detection on subsequent runs.
 */
function copyLatestVersion(siteDir, version, contentChanged) {
  const latestDir = path.join(siteDir, 'latest');
  const versionDir = path.join(siteDir, version);

  const enSrc = path.join(versionDir, 'index.html');
  const enDest = path.join(latestDir, 'index.html');
  if (fs.existsSync(enSrc)) {
    fs.mkdirSync(latestDir, { recursive: true });
    fs.copyFileSync(enSrc, enDest);
    console.log(`  Copied: ${enDest}`);
  }

  const zhSrc = path.join(versionDir, 'zh', 'index.html');
  const zhDest = path.join(latestDir, 'zh', 'index.html');
  if (fs.existsSync(zhSrc)) {
    fs.mkdirSync(path.join(latestDir, 'zh'), { recursive: true });
    fs.copyFileSync(zhSrc, zhDest);
    console.log(`  Copied: ${zhDest}`);
  }

  // Copy markdown sources to latest/ for change detection (only when content changed)
  if (contentChanged) {
    fs.mkdirSync(latestDir, { recursive: true });
    if (fs.existsSync(SPEC_FILE_EN)) {
      const mdEnDest = path.join(latestDir, 'pagx_spec.md');
      fs.copyFileSync(SPEC_FILE_EN, mdEnDest);
      console.log(`  Copied: ${mdEnDest}`);
    }
    if (fs.existsSync(SPEC_FILE_ZH)) {
      const mdZhDest = path.join(latestDir, 'pagx_spec.zh_CN.md');
      fs.copyFileSync(SPEC_FILE_ZH, mdZhDest);
      console.log(`  Copied: ${mdZhDest}`);
    }
  }
}

/**
 * Publish spec documents. Always rebuilds HTML from markdown and template.
 * Compares source markdown against cached copies only to determine whether
 * the "last updated" timestamp should be refreshed.
 */
function publishSpecDocs(outputDir) {
  const version = getSpecVersion();
  const stableVersion = getSpecStableVersion();

  console.log(`  Spec version: ${version}`);
  console.log(`  Spec stable: ${stableVersion || '(none)'}`);

  // Check whether markdown content has changed (for timestamp decision only)
  const latestDir = path.join(outputDir, 'latest');
  const cachedEn = path.join(latestDir, 'pagx_spec.md');
  const cachedZh = path.join(latestDir, 'pagx_spec.zh_CN.md');
  const contentChanged = !filesEqual(SPEC_FILE_EN, cachedEn) || !filesEqual(SPEC_FILE_ZH, cachedZh);

  // Determine timestamps: refresh only when document content actually changed
  const baseOutputDir = path.join(outputDir, version);
  const enHtmlPath = path.join(baseOutputDir, 'index.html');
  const zhHtmlPath = path.join(baseOutputDir, 'zh', 'index.html');
  let lastUpdatedEn;
  let lastUpdatedZh;

  if (contentChanged) {
    const now = new Date();
    lastUpdatedEn = formatDateEn(now);
    lastUpdatedZh = formatDateZh(now);
  } else {
    lastUpdatedEn = extractDate(enHtmlPath) || formatDateEn(new Date());
    lastUpdatedZh = extractDate(zhHtmlPath) || formatDateZh(new Date());
  }

  // Always rebuild HTML (template or generation logic may have changed)
  const viewerUrlFromRoot = '../';
  const viewerUrlFromZh = '../../';
  const faviconUrlFromRoot = '../favicon.png';
  const faviconUrlFromZh = '../../favicon.png';

  let englishSlugs = null;
  if (fs.existsSync(SPEC_FILE_EN)) {
    englishSlugs = extractEnglishSlugs(fs.readFileSync(SPEC_FILE_EN, 'utf-8'));
    console.log(`  Extracted ${englishSlugs.length} heading slugs from English version`);
  }

  console.log('\n  Publishing English spec...');
  publishSpecFile(SPEC_FILE_EN, baseOutputDir, 'en', 'zh/', viewerUrlFromRoot, faviconUrlFromRoot, englishSlugs);

  console.log('\n  Publishing Chinese spec...');
  publishSpecFile(SPEC_FILE_ZH, path.join(baseOutputDir, 'zh'), 'zh', '../', viewerUrlFromZh, faviconUrlFromZh, englishSlugs);

  // Update version links in all published versions
  updateAllVersionLinks(outputDir, version, stableVersion, lastUpdatedEn, lastUpdatedZh);

  console.log('\n  Copying latest version to latest folder...');
  copyLatestVersion(outputDir, stableVersion || version, contentChanged);
}

// ============================================================================
// Main
// ============================================================================

function main() {
  const { outputDir, skipBuild } = parseArgs();

  console.log('Publishing PAGX Playground...');
  console.log(`Output: ${outputDir}\n`);

  // Build release (uses cache if available)
  if (!skipBuild) {
    console.log('Step 1: Build release...');
    runCommand('npm run build:release', PLAYGROUND_DIR, 600000);
  } else {
    console.log('Step 1: Skipping build (--skip-build flag set)');
    if (!fs.existsSync(path.join(PLAYGROUND_DIR, 'wasm-mt', 'pagx-viewer.wasm'))) {
      console.error('ERROR: wasm-mt/pagx-viewer.wasm not found. Run build first or remove --skip-build flag');
      process.exit(1);
    }
  }

  // Copy index.html
  console.log('\nStep 2: Copy files...');
  const staticDir = path.join(PLAYGROUND_DIR, 'static');
  copyFile(
    path.join(staticDir, 'index.html'),
    path.join(outputDir, 'index.html')
  );

  // Copy index.css
  copyFile(
    path.join(staticDir, 'index.css'),
    path.join(outputDir, 'index.css')
  );

  // Copy favicon and logo
  copyFile(
    path.join(staticDir, 'favicon.png'),
    path.join(outputDir, 'favicon.png')
  );
  copyFile(
    path.join(staticDir, 'logo.png'),
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
    path.join(wasmDir, 'pagx-viewer.esm.js'),
    path.join(wasmOutputDir, 'pagx-viewer.esm.js')
  );
  copyFile(
    path.join(wasmDir, 'pagx-viewer.wasm'),
    path.join(wasmOutputDir, 'pagx-viewer.wasm')
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
      const baselineFile = 'spec_' + baseName + '_base.webp';
      const src = path.join(testOutputDir, baselineFile);
      if (fs.existsSync(src)) {
        copyFile(src, path.join(imagesOutputDir, baseName + '.webp'));
      }
    }
  } else {
    console.warn('  Warning: test output directory not found at', testOutputDir);
  }

  // Publish skills
  console.log('\nStep 3: Publish skills...');
  publishSkills(outputDir, ['pagx']);

  // Publish spec documents
  console.log('\nStep 4: Publish spec documents...');
  publishSpecDocs(outputDir);

  console.log('\nDone!');
}

main();
