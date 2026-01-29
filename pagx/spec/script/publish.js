#!/usr/bin/env node
/**
 * PAGX Specification Publisher
 *
 * Converts the PAGX specification Markdown documents to standalone HTML pages
 * with syntax highlighting and optional draft banner.
 *
 * Source files:
 *     pagx_spec.md        - English version
 *     pagx_spec.zh_CN.md  - Chinese version
 *
 * Output structure:
 *     ../public/<version>/index.html     - English (default)
 *     ../public/<version>/zh/index.html  - Chinese
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

// Default paths
const SCRIPT_DIR = __dirname;
const SPEC_DIR = path.dirname(SCRIPT_DIR);
const PAGX_DIR = path.dirname(SPEC_DIR);
const SPEC_FILE_EN = path.join(SPEC_DIR, 'pagx_spec.md');
const SPEC_FILE_ZH = path.join(SPEC_DIR, 'pagx_spec.zh_CN.md');
const PACKAGE_FILE = path.join(SPEC_DIR, 'package.json');
const SITE_DIR = path.join(PAGX_DIR, 'public');

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
  } else {
    return `${year} 年 ${month} 月 ${day} 日`;
  }
}

/**
 * Convert heading text to URL-friendly slug.
 */
function slugify(text) {
  let slug = text.toLowerCase();
  slug = slug.replace(/[^\w\u4e00-\u9fff\s-]/g, '');
  slug = slug.replace(/[\s_]+/g, '-');
  slug = slug.replace(/-+/g, '-');
  slug = slug.replace(/^-|-$/g, '');
  return slug || 'section';
}

/**
 * Extract headings from Markdown for TOC generation.
 */
function parseMarkdownHeadings(content) {
  const headings = [];
  const slugCounts = {};
  const lines = content.split('\n');

  for (const line of lines) {
    const match = line.match(/^(#{1,6})\s+(.+)$/);
    if (match) {
      const level = match[1].length;
      const text = match[2].trim();
      let slug = slugify(text);

      // Handle duplicate slugs
      if (slugCounts[slug] !== undefined) {
        slugCounts[slug]++;
        slug = `${slug}-${slugCounts[slug]}`;
      } else {
        slugCounts[slug] = 0;
      }

      headings.push({ level, text, slug });
    }
  }
  return headings;
}

/**
 * Generate HTML for table of contents.
 */
function generateTocHtml(headings) {
  if (!headings.length) {
    return '';
  }

  const html = ['<ul>'];
  const stack = [1];

  for (let i = 0; i < headings.length; i++) {
    const heading = headings[i];
    const level = heading.level;

    // Skip h1 in TOC since it's the document title
    if (level === 1) {
      continue;
    }

    // Adjust for starting from h2
    const adjustedLevel = level - 1;

    // Close lists if going up
    while (stack.length > adjustedLevel) {
      html.push('</ul></li>');
      stack.pop();
    }

    // Open new lists if going down
    while (stack.length < adjustedLevel) {
      html.push('<li><ul>');
      stack.push(stack.length + 1);
    }

    // Add the item
    html.push(`<li><a href="#${heading.slug}">${heading.text}</a>`);

    // Check if next heading is a child
    const nextIdx = i + 1;
    if (nextIdx < headings.length && headings[nextIdx].level > level) {
      html.push('<ul>');
      stack.push(stack.length + 1);
    } else {
      html.push('</li>');
    }
  }

  // Close remaining lists
  while (stack.length > 1) {
    html.push('</ul></li>');
    stack.pop();
  }

  html.push('</ul>');
  return html.join('\n');
}

/**
 * Create configured Marked instance with syntax highlighting.
 */
function createMarkedInstance() {
  const slugCounts = {};

  const marked = new Marked(
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
      // Custom slug function to handle Chinese characters
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

  marked.setOptions({
    gfm: true,
    breaks: false,
  });

  return marked;
}

/**
 * Generate the complete HTML document.
 */
function generateHtml(content, title, tocHtml, lang, showDraft, langSwitchUrl, viewerUrl, faviconUrl) {
  const isEnglish = lang === 'en';
  const htmlLang = isEnglish ? 'en' : 'zh-CN';
  const tocTitle = isEnglish ? 'Table of Contents' : '目录';
  const viewerLabel = isEnglish ? 'Viewer' : '在线预览';
  
  let draftBanner = '';
  let draftStyles = '';

  if (showDraft) {
    const draftText = isEnglish
      ? '<strong>DRAFT</strong> This specification is a working draft and may change at any time.'
      : '<strong>草案</strong> 本规范为草稿版本，内容可能随时变更。';
    draftBanner = `    <div class="draft-banner">
        ${draftText}
    </div>
`;
    draftStyles = `        .sidebar {
            top: 42px;
            height: calc(100vh - 42px);
        }
        .content {
            padding-top: 82px;
        }
        .header-actions {
            top: 54px;
        }
        @media (max-width: 900px) {
            .content {
                padding-top: 62px;
            }
        }`;
  }

  return `<!DOCTYPE html>
<html lang="${htmlLang}">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>${title}</title>
    <link rel="icon" href="${faviconUrl}" type="image/x-icon">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github.min.css">
    <style>
        :root {
            --primary-color: #0066cc;
            --bg-color: #ffffff;
            --text-color: #333333;
            --code-bg: #f6f8fa;
            --border-color: #e1e4e8;
            --toc-width: 280px;
        }
        * {
            box-sizing: border-box;
        }
        html {
            scroll-behavior: smooth;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
            line-height: 1.6;
            color: var(--text-color);
            background-color: var(--bg-color);
            margin: 0;
            padding: 0;
        }
        .container {
            display: flex;
            max-width: 1400px;
            margin: 0 auto;
        }
        .sidebar {
            width: var(--toc-width);
            position: fixed;
            top: 0;
            left: max(0px, calc((100vw - 1400px) / 2));
            height: 100vh;
            overflow-y: auto;
            padding: 20px;
            background: #f8f9fa;
            border-right: 1px solid var(--border-color);
        }
        .sidebar h2 {
            font-size: 14px;
            text-transform: uppercase;
            color: #666;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 1px solid var(--border-color);
        }
        .sidebar ul {
            list-style: none;
            padding: 0;
            margin: 0;
        }
        .sidebar li {
            margin: 4px 0;
        }
        .sidebar a {
            color: #555;
            text-decoration: none;
            font-size: 13px;
            display: block;
            padding: 4px 8px;
            border-radius: 4px;
            transition: all 0.2s;
        }
        .sidebar a:hover {
            background: #e9ecef;
            color: var(--primary-color);
        }
        .sidebar ul ul {
            padding-left: 15px;
        }
        .sidebar ul ul a {
            font-size: 12px;
            color: #777;
        }
        .content {
            flex: 1;
            margin-left: var(--toc-width);
            padding: 40px 60px;
            max-width: calc(100% - var(--toc-width));
        }
        h1, h2, h3, h4, h5, h6 {
            margin-top: 24px;
            margin-bottom: 16px;
            font-weight: 600;
            line-height: 1.25;
        }
        h1 {
            font-size: 2em;
            padding-bottom: 0.3em;
            border-bottom: 1px solid var(--border-color);
            margin-bottom: 16px;
        }
        .last-updated {
            margin-top: 0;
            margin-bottom: 24px;
            color: #6a737d;
            font-size: 14px;
        }
        h2 {
            font-size: 1.5em;
            padding-bottom: 0.3em;
            border-bottom: 1px solid var(--border-color);
        }
        h3 { font-size: 1.25em; }
        h4 { font-size: 1em; }
        h5 { font-size: 0.875em; }
        h6 { font-size: 0.85em; color: #6a737d; }
        
        a {
            color: var(--primary-color);
            text-decoration: none;
        }
        a:hover {
            text-decoration: underline;
        }
        p {
            margin-top: 0;
            margin-bottom: 16px;
        }
        code {
            font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace;
            font-size: 85%;
            background-color: var(--code-bg);
            padding: 0.2em 0.4em;
            border-radius: 6px;
        }
        pre {
            background-color: var(--code-bg);
            border-radius: 6px;
            padding: 16px;
            overflow: auto;
            font-size: 85%;
            line-height: 1.45;
            border: 1px solid var(--border-color);
        }
        pre code {
            background-color: transparent;
            padding: 0;
            border-radius: 0;
            font-size: 100%;
        }
        table {
            border-collapse: collapse;
            width: 100%;
            margin: 16px 0;
            overflow: auto;
        }
        th, td {
            border: 1px solid var(--border-color);
            padding: 8px 13px;
            text-align: left;
        }
        th {
            background-color: #f6f8fa;
            font-weight: 600;
        }
        tr:nth-child(even) {
            background-color: #f9f9f9;
        }
        blockquote {
            margin: 0;
            padding: 0 1em;
            color: #6a737d;
            border-left: 0.25em solid #dfe2e5;
        }
        hr {
            height: 0.25em;
            padding: 0;
            margin: 24px 0;
            background-color: var(--border-color);
            border: 0;
        }
        ul, ol {
            padding-left: 2em;
            margin-top: 0;
            margin-bottom: 16px;
        }
        li {
            margin: 4px 0;
        }
        li > ul, li > ol {
            margin-top: 4px;
            margin-bottom: 0;
        }
        strong {
            font-weight: 600;
        }
        img {
            max-width: 100%;
            box-sizing: content-box;
        }
        .hljs {
            background: var(--code-bg) !important;
        }
        @media (max-width: 900px) {
            .sidebar {
                display: none;
            }
            .content {
                margin-left: 0;
                max-width: 100%;
                padding: 20px;
            }
        }
        /* Back to top button */
        .back-to-top {
            position: fixed;
            bottom: 30px;
            right: 30px;
            width: 40px;
            height: 40px;
            background: var(--primary-color);
            color: white;
            border: none;
            border-radius: 50%;
            cursor: pointer;
            opacity: 0;
            transition: opacity 0.3s;
            font-size: 20px;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .back-to-top.visible {
            opacity: 1;
        }
        .back-to-top:hover {
            background: #0052a3;
        }
        /* Draft banner */
        .draft-banner {
            background-color: #fef9e7;
            border-bottom: 1px solid #f5e6b3;
            padding: 10px 20px;
            text-align: center;
            font-size: 14px;
            color: #7d6608;
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            z-index: 1000;
        }
        .draft-banner strong {
            color: #5a4a06;
        }
        .lang-switch {
            position: relative;
        }
        .lang-switch-btn {
            display: flex;
            align-items: center;
            gap: 6px;
            padding: 8px 14px;
            background: #f8f9fa;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            color: #555;
            font-family: inherit;
            font-size: 14px;
            line-height: 1.2;
            cursor: pointer;
            transition: all 0.2s;
        }
        .lang-switch-btn:hover {
            background: #e9ecef;
        }
        .lang-switch-btn svg {
            width: 14px;
            height: 14px;
            transition: transform 0.2s;
        }
        .lang-switch.open .lang-switch-btn svg {
            transform: rotate(180deg);
        }
        .lang-switch-menu {
            position: absolute;
            top: 100%;
            right: 0;
            min-width: 100%;
            margin-top: 4px;
            background: white;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.1);
            overflow: hidden;
            opacity: 0;
            visibility: hidden;
            transform: translateY(-8px);
            transition: all 0.2s;
        }
        .lang-switch.open .lang-switch-menu {
            opacity: 1;
            visibility: visible;
            transform: translateY(0);
        }
        .lang-switch-menu a {
            display: block;
            padding: 8px 16px;
            color: #555;
            font-size: 13px;
            text-decoration: none;
            white-space: nowrap;
        }
        .lang-switch-menu a:hover {
            background: #f8f9fa;
            color: var(--primary-color);
        }
        .lang-switch-menu a.active {
            background: #e3f2fd;
            color: #1565c0;
        }
        /* Header buttons container */
        .header-actions {
            position: fixed;
            top: 12px;
            right: 20px;
            z-index: 1001;
            display: flex;
            align-items: center;
            gap: 8px;
        }
        /* Viewer link */
        .viewer-link {
            display: flex;
            align-items: center;
            gap: 6px;
            padding: 8px 14px;
            background: #f8f9fa;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            color: #555;
            font-family: inherit;
            font-size: 14px;
            line-height: 1.2;
            text-decoration: none;
            transition: all 0.2s;
        }
        .viewer-link:hover {
            background: #e9ecef;
            text-decoration: none;
        }
        .viewer-link svg {
            width: 14px;
            height: 14px;
        }
${draftStyles}
    </style>
</head>
<body>
${draftBanner}    <div class="header-actions">
        <a href="${viewerUrl}" class="viewer-link" target="_blank">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="5 3 19 12 5 21 5 3"></polygon></svg>
            ${viewerLabel}
        </a>
        <div class="lang-switch" onclick="this.classList.toggle('open')">
            <button class="lang-switch-btn">
                ${isEnglish ? 'English' : '简体中文'}
                <svg viewBox="0 0 12 12" fill="currentColor"><path d="M2.5 4.5L6 8L9.5 4.5" stroke="currentColor" stroke-width="1.5" fill="none"/></svg>
            </button>
            <div class="lang-switch-menu">
                <a href="${isEnglish ? '#' : langSwitchUrl}"${isEnglish ? ' class="active"' : ''}>English</a>
                <a href="${isEnglish ? langSwitchUrl : '#'}"${isEnglish ? '' : ' class="active"'}>简体中文</a>
            </div>
        </div>
    </div>
    <div class="container">
        <nav class="sidebar">
            <h2>${tocTitle}</h2>
            <div class="toc">
${tocHtml}
            </div>
        </nav>
        <main class="content">
            ${content}
        </main>
    </div>
    <button class="back-to-top" onclick="window.scrollTo({top: 0, behavior: 'smooth'})">↑</button>
    
    <script>
        // Back to top button visibility
        window.addEventListener('scroll', function() {
            const btn = document.querySelector('.back-to-top');
            if (window.scrollY > 300) {
                btn.classList.add('visible');
            } else {
                btn.classList.remove('visible');
            }
        });
        // Close language menu when clicking outside
        document.addEventListener('click', function(e) {
            const langSwitch = document.querySelector('.lang-switch');
            if (!langSwitch.contains(e.target)) {
                langSwitch.classList.remove('open');
            }
        });
    </script>
</body>
</html>`;
}

/**
 * Publish a single spec file.
 */
function publishSpec(specFile, outputDir, lang, version, showDraft, langSwitchUrl, viewerUrl, faviconUrl) {
  if (!fs.existsSync(specFile)) {
    console.log(`  Skipped (file not found): ${specFile}`);
    return;
  }

  console.log(`  Reading: ${specFile}`);
  let mdContent = fs.readFileSync(specFile, 'utf-8');

  // Get file last modified time
  const stats = fs.statSync(specFile);
  const lastModified = stats.mtime;

  // Format date based on language
  const dateStr = formatDate(lastModified, lang);

  // Generate version info HTML
  const isEnglish = lang === 'en';
  const lastUpdatedLabel = isEnglish ? `Last updated: ${dateStr}` : `最后更新：${dateStr}`;

  // Inject version into title and add last updated below
  mdContent = mdContent.replace(/^(#\s+.+)$/m, `$1 ${version}\n\n<p class="last-updated">${lastUpdatedLabel}</p>`);

  // Extract title
  const titleMatch = mdContent.match(/^#\s+(.+)$/m);
  const title = titleMatch ? titleMatch[1] : 'PAGX Format Specification';

  // Parse headings for TOC
  const headings = parseMarkdownHeadings(mdContent);

  // Generate TOC HTML
  const tocHtml = generateTocHtml(headings);

  // Convert Markdown to HTML using marked
  const marked = createMarkedInstance();
  const htmlContent = marked.parse(mdContent);

  // Generate complete HTML document
  const html = generateHtml(htmlContent, title, tocHtml, lang, showDraft, langSwitchUrl, viewerUrl, faviconUrl);

  // Create output directory
  fs.mkdirSync(outputDir, { recursive: true });

  // Write output file
  const outputFile = path.join(outputDir, 'index.html');
  fs.writeFileSync(outputFile, html, 'utf-8');

  console.log(`  Published: ${outputFile}`);
}

/**
 * Parse command line arguments.
 */
function parseArgs() {
  const args = process.argv.slice(2);
  const options = {};

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === '--help' || arg === '-h') {
      console.log(`
PAGX Specification Publisher

Usage:
    cd pagx/spec && npm run publish

Configuration (package.json):
    version        - Current version to publish
    stableVersion  - Latest stable version (empty if none)
                     If version != stableVersion, draft banner is shown

Source files:
    spec/pagx_spec.md        - English version
    spec/pagx_spec.zh_CN.md  - Chinese version

Output structure:
    public/index.html               - Redirect page
    public/<version>/index.html     - English (default)
    public/<version>/zh/index.html  - Chinese

Examples:
    npm run publish:spec
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
  const options = parseArgs();

  // Read versions from package.json
  const version = getVersion();
  const stableVersion = getStableVersion();
  const isDraft = version !== stableVersion;

  console.log(`Version: ${version}`);
  console.log(`Stable: ${stableVersion || '(none)'}`);
  if (isDraft) {
    console.log('Mode: Draft');
  }

  const baseOutputDir = path.join(SITE_DIR, version);

  // Viewer URL (relative path from spec pages to viewer)
  const viewerUrlFromRoot = '../viewer/';
  const viewerUrlFromZh = '../../viewer/';

  // Favicon URL (relative path from spec pages to favicon)
  const faviconUrlFromRoot = '../favicon.ico';
  const faviconUrlFromZh = '../../favicon.ico';

  // Publish English version (default, at root)
  console.log('\nPublishing English version...');
  publishSpec(SPEC_FILE_EN, baseOutputDir, 'en', version, isDraft, 'zh/', viewerUrlFromRoot, faviconUrlFromRoot);

  // Publish Chinese version (under /zh/)
  console.log('\nPublishing Chinese version...');
  publishSpec(SPEC_FILE_ZH, path.join(baseOutputDir, 'zh'), 'zh', version, isDraft, '../', viewerUrlFromZh, faviconUrlFromZh);

  // Generate redirect index page (point to stableVersion if exists, otherwise current version)
  const redirectVersion = stableVersion || version;
  console.log('\nGenerating redirect page...');
  console.log(`  Redirect to: ${redirectVersion}`);
  generateRedirectPage(redirectVersion);

  // Copy favicon
  console.log('\nCopying favicon...');
  const faviconSrc = path.join(SPEC_DIR, 'favicon.ico');
  const faviconDest = path.join(SITE_DIR, 'favicon.ico');
  fs.copyFileSync(faviconSrc, faviconDest);
  console.log(`  Copied: ${faviconDest}`);

  console.log('\nDone!');
}

/**
 * Generate the redirect index page at site/index.html.
 */
function generateRedirectPage(version) {
  const html = `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PAGX Specification</title>
    <script>
        (function() {
            var version = '${version}';
            
            // Check browser language
            var lang = navigator.language || navigator.userLanguage || '';
            var isChinese = lang.toLowerCase().startsWith('zh');
            
            // Build redirect URL
            var path = version + '/';
            if (isChinese) {
                path += 'zh/';
            }
            
            // Redirect
            window.location.replace(path);
        })();
    </script>
</head>
<body>
    <p>Redirecting to the latest specification...</p>
</body>
</html>
`;

  fs.mkdirSync(SITE_DIR, { recursive: true });
  const outputFile = path.join(SITE_DIR, 'index.html');
  fs.writeFileSync(outputFile, html, 'utf-8');
  console.log(`  Generated: ${outputFile}`);
}

main();
