#!/usr/bin/env node
/**
 * PAGX Specification Publisher
 *
 * Converts the PAGX specification Markdown documents to standalone HTML pages.
 *
 * Source files:
 *     pagx_spec.md        - English version
 *     pagx_spec.zh_CN.md  - Chinese version
 *
 * Usage:
 *     cd pagx/spec && npm run publish
 */

const fs = require('fs');
const path = require('path');
const { Marked } = require('marked');
const { markedHighlight } = require('marked-highlight');
const { gfmHeadingId } = require('marked-gfm-heading-id');
const hljs = require('highlight.js');

// Paths
const SCRIPT_DIR = __dirname;
const SPEC_DIR = path.dirname(SCRIPT_DIR);
const PAGX_DIR = path.dirname(SPEC_DIR);
const SPEC_FILE_EN = path.join(SPEC_DIR, 'pagx_spec.md');
const SPEC_FILE_ZH = path.join(SPEC_DIR, 'pagx_spec.zh_CN.md');
const PACKAGE_FILE = path.join(SPEC_DIR, 'package.json');
const TEMPLATE_FILE = path.join(SCRIPT_DIR, 'template.html');
const DEFAULT_SITE_DIR = path.join(PAGX_DIR, 'public');

// Base URL for the spec site
const BASE_URL = 'https://pag.io/pagx';

// Load template
const TEMPLATE = fs.readFileSync(TEMPLATE_FILE, 'utf-8');

/**
 * Read version from package.json.
 */
function getVersion() {
  const pkg = JSON.parse(fs.readFileSync(PACKAGE_FILE, 'utf-8'));
  return pkg.version;
}

/**
 * Read stable version from package.json.
 */
function getStableVersion() {
  const pkg = JSON.parse(fs.readFileSync(PACKAGE_FILE, 'utf-8'));
  return pkg.stableVersion || '';
}

/**
 * Format date based on language.
 */
function formatDate(date, lang) {
  const year = date.getFullYear();
  const month = date.getMonth() + 1;
  const day = date.getDate();

  if (lang === 'en') {
    const months = ['January', 'February', 'March', 'April', 'May', 'June',
                    'July', 'August', 'September', 'October', 'November', 'December'];
    return `${day} ${months[month - 1]} ${year}`;
  }
  return `${year} 年 ${month} 月 ${day} 日`;
}

/**
 * Convert heading text to URL-friendly slug.
 * Removes leading section numbers (e.g., "3.3.5 Font" -> "font").
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
 * Appendix headings are prefixed with "ref-" to avoid conflicts.
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
 * Generate HTML document from template.
 */
function generateHtml(content, title, tocHtml, lang, langSwitchUrl, viewerUrl, faviconUrl) {
  const isEnglish = lang === 'en';

  const vars = {
    htmlLang: isEnglish ? 'en' : 'zh-CN',
    title,
    faviconUrl,
    viewerUrl,
    tocTitle: isEnglish ? 'Table of Contents' : '目录',
    tocLabel: isEnglish ? 'TOC' : '目录',
    viewerLabel: isEnglish ? 'Viewer' : '在线预览',
    currentLang: isEnglish ? 'English' : '简体中文',
    enUrl: isEnglish ? '#' : langSwitchUrl,
    zhUrl: isEnglish ? langSwitchUrl : '#',
    enActive: isEnglish ? ' class="active"' : '',
    zhActive: isEnglish ? '' : ' class="active"',
    tocHtml,
    content,
  };

  return TEMPLATE.replace(/\{\{(\w+)\}\}/g, (_, key) => vars[key] || '');
}

/**
 * Publish a single spec file.
 */
function publishSpec(specFile, outputDir, lang, langSwitchUrl, viewerUrl, faviconUrl, englishSlugs = null) {
  if (!fs.existsSync(specFile)) {
    console.log(`  Skipped (file not found): ${specFile}`);
    return;
  }

  console.log(`  Reading: ${specFile}`);
  const mdContent = fs.readFileSync(specFile, 'utf-8');

  const titleMatch = mdContent.match(/^#\s+(.+)$/m);
  const title = titleMatch ? titleMatch[1] : 'PAGX Format Specification';

  const headings = parseMarkdownHeadings(mdContent, englishSlugs);
  const tocHtml = generateTocHtml(headings);

  const marked = createMarkedInstance();
  let htmlContent = marked.parse(mdContent);

  if (englishSlugs) {
    htmlContent = replaceHeadingIds(htmlContent, englishSlugs);
  }

  const html = generateHtml(htmlContent, title, tocHtml, lang, langSwitchUrl, viewerUrl, faviconUrl);

  fs.mkdirSync(outputDir, { recursive: true });
  const outputFile = path.join(outputDir, 'index.html');
  fs.writeFileSync(outputFile, html, 'utf-8');
  console.log(`  Published: ${outputFile}`);
}

/**
 * Generate redirect page in latest folder.
 */
function generateRedirectPage(siteDir, version) {
  const html = `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PAGX Specification</title>
    <script>
        (function() {
            var lang = navigator.language || navigator.userLanguage || '';
            var path = '../${version}/' + (lang.toLowerCase().startsWith('zh') ? 'zh/' : '');
            window.location.replace(path);
        })();
    </script>
</head>
<body>
    <p>Redirecting to the latest specification...</p>
</body>
</html>`;

  const latestDir = path.join(siteDir, 'latest');
  fs.mkdirSync(latestDir, { recursive: true });
  const outputFile = path.join(latestDir, 'index.html');
  fs.writeFileSync(outputFile, html, 'utf-8');
  console.log(`  Generated: ${outputFile}`);
}

/**
 * Find all published versions in the site directory.
 */
function findPublishedVersions(siteDir) {
  if (!fs.existsSync(siteDir)) return [];

  const entries = fs.readdirSync(siteDir, { withFileTypes: true });
  const versions = [];

  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    // Skip non-version directories
    if (['latest', 'fonts', 'wasm-mt', 'node_modules', 'viewer'].includes(entry.name)) continue;
    // Version directories must match semver pattern (e.g., 1.0, 0.5, 2.1.3)
    if (!/^\d+\.\d+(\.\d+)?$/.test(entry.name)) continue;
    // Check if it looks like a version (contains index.html)
    const indexPath = path.join(siteDir, entry.name, 'index.html');
    if (fs.existsSync(indexPath)) {
      versions.push(entry.name);
    }
  }

  return versions;
}

/**
 * Generate version info block HTML (left border style).
 * - Stable (latest): green border
 * - Draft: yellow border with draft warning
 * - Outdated: red border with outdated warning
 */
function generateVersionInfoHtml(thisVersion, draftVersion, stableVersion, isZh, lastUpdated) {
  const langSuffix = isZh ? 'zh/' : '';
  const thisUrl = `${BASE_URL}/${thisVersion}/${langSuffix}`;
  const latestUrl = `${BASE_URL}/latest/`;

  // Check version status
  const isOutdated = stableVersion && thisVersion !== stableVersion &&
    thisVersion.localeCompare(stableVersion, undefined, { numeric: true }) < 0;
  const isDraft = !isOutdated && thisVersion !== stableVersion;

  // Determine status class and text
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

  // Generate labels
  const labels = isZh
    ? { updated: '最后更新：', this: '当前版本：', latest: '最新版本：', draft: '草稿版本：' }
    : { updated: 'Last updated:', this: 'This version:', latest: 'Latest version:', draft: "Editor's draft:" };

  // Generate links
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
 * Update version links in an HTML file.
 * Removes old version links block and inserts new one after the h1 title.
 * Preserves the original "last updated" time from the existing HTML.
 */
function updateVersionLinks(htmlFile, thisVersion, draftVersion, stableVersion, isZh) {
  if (!fs.existsSync(htmlFile)) return false;

  let html = fs.readFileSync(htmlFile, 'utf-8');

  // Extract existing "last updated" date from HTML, preserve it
  // Matches: "30 January 2026" or "2026 年 1 月 30 日"
  const lastUpdatedRegex = /(\d{1,2}\s+\w+\s+\d{4}|\d{4}\s*年\s*\d{1,2}\s*月\s*\d{1,2}\s*日)/;
  const lastUpdatedMatch = html.match(lastUpdatedRegex);
  const lastUpdated = lastUpdatedMatch ? lastUpdatedMatch[1] : formatDate(new Date(), isZh ? 'zh' : 'en');

  // Generate version info block
  const versionInfoHtml = generateVersionInfoHtml(thisVersion, draftVersion, stableVersion, isZh, lastUpdated);


  // Update CSS: replace old version-info styles with new ones (with background colors)
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

  // Replace existing version-info CSS block or insert new one
  if (html.includes('/* Version info block */')) {
    html = html.replace(
      /\/\* Version info block \*\/[\s\S]*?\.version-info p \{[^}]+\}/,
      versionInfoCss
    );
  } else {
    // Insert after img styles
    html = html.replace(
      /(img \{[^}]+\})/,
      `$1\n\n        ${versionInfoCss}`
    );
  }

  // Restore h1 border-bottom if it was removed
  html = html.replace(
    /h1 \{ font-size: 2em; margin-bottom: 16px; \}/,
    'h1 { font-size: 2em; padding-bottom: 0.3em; border-bottom: 1px solid var(--border-color); margin-bottom: 16px; }'
  );


  // Pattern to match everything from h1 closing tag to first h2 opening tag
  const versionBlockRegex = /(<h1[^>]*>)[^<]*(<\/h1>)[\s\S]*?(<h2)/;

  const match = html.match(versionBlockRegex);
  if (match) {
    const h1Open = match[1];
    const h1Close = match[2];
    const h2Open = match[3];

    // Get the title text between h1 tags
    const titleMatch = html.match(/<h1[^>]*>([^<]*)<\/h1>/);
    let title = titleMatch ? titleMatch[1] : '';

    // Remove version number from title if present
    title = title.replace(/\s+\d+\.\d+(\.\d+)?$/, '');

    // Also update the h1 id
    const cleanH1Open = h1Open.replace(/id="[^"]*"/, 'id="pagx-format-specification"');

    // Structure: title -> version info block -> content
    const newContent = `${cleanH1Open}${title}${h1Close}\n${versionInfoHtml}\n${h2Open}`;
    html = html.replace(versionBlockRegex, newContent);
    fs.writeFileSync(htmlFile, html, 'utf-8');
    return true;
  }
  return false;
}

/**
 * Update version links in all published versions.
 */
function updateAllVersionLinks(siteDir, draftVersion, stableVersion) {
  const versions = findPublishedVersions(siteDir);
  if (versions.length === 0) return;

  console.log('\nUpdating version links in all versions...');

  for (const ver of versions) {
    const enFile = path.join(siteDir, ver, 'index.html');
    const zhFile = path.join(siteDir, ver, 'zh', 'index.html');

    if (updateVersionLinks(enFile, ver, draftVersion, stableVersion, false)) {
      console.log(`  Updated: ${enFile}`);
    }
    if (updateVersionLinks(zhFile, ver, draftVersion, stableVersion, true)) {
      console.log(`  Updated: ${zhFile}`);
    }
  }
}

/**
 * Parse command line arguments.
 */
function parseArgs() {
  const args = process.argv.slice(2);
  const options = { siteDir: DEFAULT_SITE_DIR };

  for (let i = 0; i < args.length; i++) {
    if ((args[i] === '-o' || args[i] === '--output') && args[i + 1]) {
      options.siteDir = path.resolve(args[++i]);
    } else if (args[i] === '-h' || args[i] === '--help') {
      console.log(`
PAGX Specification Publisher

Usage: npm run publish [-- -o <output-dir>]

Options:
    -o, --output <dir>  Output directory (default: ../public)
    -h, --help          Show this help message
`);
      process.exit(0);
    }
  }
  return options;
}

/**
 * Main function.
 */
function main() {
  const { siteDir } = parseArgs();
  const version = getVersion();
  const stableVersion = getStableVersion();

  console.log(`Version: ${version}`);
  console.log(`Stable: ${stableVersion || '(none)'}`);
  console.log(`Output: ${siteDir}`);

  const baseOutputDir = path.join(siteDir, version);
  const viewerUrlFromRoot = '../';
  const viewerUrlFromZh = '../../';
  const faviconUrlFromRoot = '../favicon.png';
  const faviconUrlFromZh = '../../favicon.png';

  let englishSlugs = null;
  if (fs.existsSync(SPEC_FILE_EN)) {
    englishSlugs = extractEnglishSlugs(fs.readFileSync(SPEC_FILE_EN, 'utf-8'));
    console.log(`\nExtracted ${englishSlugs.length} heading slugs from English version`);
  }

  console.log('\nPublishing English version...');
  publishSpec(SPEC_FILE_EN, baseOutputDir, 'en', 'zh/', viewerUrlFromRoot, faviconUrlFromRoot, englishSlugs);

  console.log('\nPublishing Chinese version...');
  publishSpec(SPEC_FILE_ZH, path.join(baseOutputDir, 'zh'), 'zh', '../', viewerUrlFromZh, faviconUrlFromZh, englishSlugs);

  console.log('\nGenerating redirect page...');
  console.log(`  Redirect to: ${stableVersion || version}`);
  generateRedirectPage(siteDir, stableVersion || version);

  // Update version links in all published versions
  updateAllVersionLinks(siteDir, version, stableVersion);

  console.log('\nCopying favicon...');
  fs.copyFileSync(path.join(SPEC_DIR, 'favicon.png'), path.join(siteDir, 'favicon.png'));
  console.log(`  Copied: ${path.join(siteDir, 'favicon.png')}`);

  console.log('\nDone!');
}

main();
