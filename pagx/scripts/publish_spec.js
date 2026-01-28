#!/usr/bin/env node
/**
 * PAGX Specification Publisher
 *
 * Converts the PAGX specification Markdown document to a standalone HTML page
 * with syntax highlighting and optional draft banner.
 *
 * Usage:
 *     node publish_spec.js [--output <dir>] [--draft]
 *
 * Options:
 *     --output, -o    Output directory (default: pagx/.spec_output/)
 *     --draft, -d     Show draft banner at the top of the page
 */

const fs = require('fs');
const path = require('path');

// Default paths
const SCRIPT_DIR = __dirname;
const PAGX_DIR = path.dirname(SCRIPT_DIR);
const SPEC_FILE = path.join(PAGX_DIR, 'docs', 'pagx_spec.md');
const DEFAULT_OUTPUT_DIR = path.join(PAGX_DIR, '.spec_output');

/**
 * Convert heading text to URL-friendly slug.
 */
function slugify(text) {
  let slug = text.toLowerCase();
  // Remove special characters but keep Chinese characters and alphanumeric
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
  const lines = content.split('\n');
  for (const line of lines) {
    const match = line.match(/^(#{1,6})\s+(.+)$/);
    if (match) {
      const level = match[1].length;
      const text = match[2].trim();
      const slug = slugify(text);
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
 * Escape HTML special characters.
 */
function escapeHtml(text) {
  return text
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#039;');
}

/**
 * Convert Markdown content to HTML.
 */
function convertMarkdownToHtml(content, headings) {
  // Create a map for heading slugs to handle duplicates
  const slugCounts = {};

  // Process code blocks first to protect them
  const codeBlocks = [];
  let processedContent = content.replace(/```(\w*)\n([\s\S]*?)```/g, (match, lang, code) => {
    const placeholder = `__CODE_BLOCK_${codeBlocks.length}__`;
    const langClass = lang ? ` class="language-${lang}"` : '';
    codeBlocks.push(`<pre><code${langClass}>${escapeHtml(code.trim())}</code></pre>`);
    return placeholder;
  });

  // Process inline code
  processedContent = processedContent.replace(/`([^`]+)`/g, '<code>$1</code>');

  // Process headings with IDs
  processedContent = processedContent.replace(/^(#{1,6})\s+(.+)$/gm, (match, hashes, text) => {
    const level = hashes.length;
    let slug = slugify(text);

    // Handle duplicate slugs
    if (slugCounts[slug] !== undefined) {
      slugCounts[slug]++;
      slug = `${slug}-${slugCounts[slug]}`;
    } else {
      slugCounts[slug] = 0;
    }

    return `<h${level} id="${slug}">${text}</h${level}>`;
  });

  // Process bold
  processedContent = processedContent.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');

  // Process italic
  processedContent = processedContent.replace(/\*([^*]+)\*/g, '<em>$1</em>');

  // Process links
  processedContent = processedContent.replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2">$1</a>');

  // Process horizontal rules
  processedContent = processedContent.replace(/^---+$/gm, '<hr>');

  // Process blockquotes
  processedContent = processedContent.replace(/^>\s*(.*)$/gm, '<blockquote>$1</blockquote>');
  // Merge adjacent blockquotes
  processedContent = processedContent.replace(/<\/blockquote>\n<blockquote>/g, '\n');

  // Process tables
  processedContent = processedContent.replace(
    /^\|(.+)\|\n\|[-:| ]+\|\n((?:\|.+\|\n?)+)/gm,
    (match, headerRow, bodyRows) => {
      const headers = headerRow.split('|').map((h) => h.trim()).filter(Boolean);
      const headerHtml = headers.map((h) => `<th>${h}</th>`).join('');

      const rows = bodyRows.trim().split('\n');
      const bodyHtml = rows
        .map((row) => {
          const cells = row.split('|').map((c) => c.trim()).filter(Boolean);
          return `<tr>${cells.map((c) => `<td>${c}</td>`).join('')}</tr>`;
        })
        .join('\n');

      return `<table>\n<thead><tr>${headerHtml}</tr></thead>\n<tbody>\n${bodyHtml}\n</tbody>\n</table>`;
    }
  );

  // Process unordered lists
  let inList = false;
  const lines = processedContent.split('\n');
  const processedLines = [];

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const listMatch = line.match(/^(\s*)[-*]\s+(.+)$/);

    if (listMatch) {
      if (!inList) {
        processedLines.push('<ul>');
        inList = true;
      }
      processedLines.push(`<li>${listMatch[2]}</li>`);
    } else {
      if (inList && line.trim() !== '') {
        processedLines.push('</ul>');
        inList = false;
      }
      processedLines.push(line);
    }
  }
  if (inList) {
    processedLines.push('</ul>');
  }
  processedContent = processedLines.join('\n');

  // Process ordered lists
  inList = false;
  const lines2 = processedContent.split('\n');
  const processedLines2 = [];

  for (let i = 0; i < lines2.length; i++) {
    const line = lines2[i];
    const listMatch = line.match(/^(\s*)\d+\.\s+(.+)$/);

    if (listMatch) {
      if (!inList) {
        processedLines2.push('<ol>');
        inList = true;
      }
      processedLines2.push(`<li>${listMatch[2]}</li>`);
    } else {
      if (inList && line.trim() !== '') {
        processedLines2.push('</ol>');
        inList = false;
      }
      processedLines2.push(line);
    }
  }
  if (inList) {
    processedLines2.push('</ol>');
  }
  processedContent = processedLines2.join('\n');

  // Process paragraphs - wrap non-HTML lines in <p> tags
  const finalLines = processedContent.split('\n');
  const result = [];
  let paragraph = [];

  for (const line of finalLines) {
    const trimmed = line.trim();

    // Check if line starts with HTML tag or is a placeholder
    const isHtml =
      trimmed.startsWith('<') ||
      trimmed.startsWith('__CODE_BLOCK_') ||
      trimmed === '';

    if (isHtml) {
      if (paragraph.length > 0) {
        result.push(`<p>${paragraph.join(' ')}</p>`);
        paragraph = [];
      }
      if (trimmed !== '') {
        result.push(line);
      }
    } else {
      paragraph.push(trimmed);
    }
  }
  if (paragraph.length > 0) {
    result.push(`<p>${paragraph.join(' ')}</p>`);
  }

  processedContent = result.join('\n');

  // Restore code blocks
  for (let i = 0; i < codeBlocks.length; i++) {
    processedContent = processedContent.replace(`__CODE_BLOCK_${i}__`, codeBlocks[i]);
  }

  return processedContent;
}

/**
 * Generate the complete HTML document.
 */
function generateHtml(content, title, tocHtml, showDraft) {
  let draftBanner = '';
  let draftStyles = '';

  if (showDraft) {
    draftBanner = `    <div class="draft-banner">
        <strong>DRAFT</strong> This specification is a working draft and may change at any time. 本规范为草稿版本，内容可能随时变更。
    </div>
`;
    draftStyles = `        .sidebar {
            top: 42px;
            height: calc(100vh - 42px);
        }
        .content {
            padding-top: 82px;
        }
        @media (max-width: 900px) {
            .content {
                padding-top: 62px;
            }
        }`;
  }

  return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>${title}</title>
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
${draftStyles}
    </style>
</head>
<body>
${draftBanner}    <div class="container">
        <nav class="sidebar">
            <h2>Table of Contents</h2>
            <div class="toc">
${tocHtml}
            </div>
        </nav>
        <main class="content">
            ${content}
        </main>
    </div>
    <button class="back-to-top" onclick="window.scrollTo({top: 0, behavior: 'smooth'})">↑</button>
    
    <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/languages/xml.min.js"></script>
    <script>
        // Initialize highlight.js
        hljs.highlightAll();
        
        // Back to top button visibility
        window.addEventListener('scroll', function() {
            const btn = document.querySelector('.back-to-top');
            if (window.scrollY > 300) {
                btn.classList.add('visible');
            } else {
                btn.classList.remove('visible');
            }
        });
    </script>
</body>
</html>`;
}

/**
 * Parse command line arguments.
 */
function parseArgs() {
  const args = process.argv.slice(2);
  const options = {
    output: DEFAULT_OUTPUT_DIR,
    draft: false,
  };

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === '--output' || arg === '-o') {
      options.output = args[++i];
    } else if (arg === '--draft' || arg === '-d') {
      options.draft = true;
    } else if (arg === '--help' || arg === '-h') {
      console.log(`
PAGX Specification Publisher

Usage:
    node publish_spec.js [--output <dir>] [--draft]

Options:
    --output, -o    Output directory (default: pagx/.spec_output/)
    --draft, -d     Show draft banner at the top of the page
    --help, -h      Show this help message

Examples:
    # Publish to default location (pagx/.spec_output/)
    node publish_spec.js

    # Publish as draft
    node publish_spec.js --draft

    # Publish to custom directory
    node publish_spec.js --output /path/to/output

    # Publish as draft to custom directory
    node publish_spec.js --draft --output /path/to/output
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

  // Check if source file exists
  if (!fs.existsSync(SPEC_FILE)) {
    console.error(`Error: Specification file not found: ${SPEC_FILE}`);
    process.exit(1);
  }

  // Read Markdown content
  console.log(`Reading: ${SPEC_FILE}`);
  const mdContent = fs.readFileSync(SPEC_FILE, 'utf-8');

  // Extract title from first heading
  const titleMatch = mdContent.match(/^#\s+(.+)$/m);
  const title = titleMatch ? titleMatch[1] : 'PAGX Format Specification';

  // Parse headings for TOC
  const headings = parseMarkdownHeadings(mdContent);

  // Generate TOC HTML
  const tocHtml = generateTocHtml(headings);

  // Convert Markdown to HTML
  const htmlContent = convertMarkdownToHtml(mdContent, headings);

  // Generate complete HTML document
  const html = generateHtml(htmlContent, title, tocHtml, options.draft);

  // Create output directory
  fs.mkdirSync(options.output, { recursive: true });

  // Write output file
  const outputFile = path.join(options.output, 'index.html');
  fs.writeFileSync(outputFile, html, 'utf-8');

  console.log(`Published to: ${outputFile}`);
  if (options.draft) {
    console.log('  (with draft banner)');
  }
}

main();
