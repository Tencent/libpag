#!/usr/bin/env python3
"""
PAGX Specification Publisher

Converts the PAGX specification Markdown document to a standalone HTML page
with syntax highlighting and optional draft banner.

Usage:
    python publish_spec.py [--output <dir>] [--draft]

Options:
    --output, -o    Output directory (default: pagx/.spec_output/)
    --draft, -d     Show draft banner at the top of the page
"""

import argparse
import os
import re
import sys
from pathlib import Path

# Default paths
SCRIPT_DIR = Path(__file__).parent
PAGX_DIR = SCRIPT_DIR.parent
SPEC_FILE = PAGX_DIR / "docs" / "pagx_spec.md"
DEFAULT_OUTPUT_DIR = PAGX_DIR / ".spec_output"


def slugify(text: str, separator: str = '-') -> str:
    """Convert heading text to URL-friendly slug."""
    # Remove special characters but keep Chinese characters
    text = text.lower()
    # Handle text with both English and Chinese
    text = re.sub(r'[^\w\u4e00-\u9fff\s-]', '', text)
    text = re.sub(r'[\s_]+', separator, text)
    text = re.sub(f'{separator}+', separator, text)
    text = text.strip(separator)
    return text if text else 'section'


def parse_markdown_headings(content: str) -> list:
    """Extract headings from Markdown for TOC generation."""
    headings = []
    lines = content.split('\n')
    for line in lines:
        match = re.match(r'^(#{1,6})\s+(.+)$', line)
        if match:
            level = len(match.group(1))
            text = match.group(2).strip()
            slug = slugify(text)
            headings.append({'level': level, 'text': text, 'slug': slug})
    return headings


def generate_toc_html(headings: list) -> str:
    """Generate HTML for table of contents."""
    if not headings:
        return ''
    
    html = ['<ul>']
    stack = [1]  # Track nesting level
    
    for heading in headings:
        level = heading['level']
        
        # Skip h1 in TOC since it's the document title
        if level == 1:
            continue
        
        # Adjust for starting from h2
        adjusted_level = level - 1
        
        # Close lists if going up
        while len(stack) > adjusted_level:
            html.append('</ul></li>')
            stack.pop()
        
        # Open new lists if going down
        while len(stack) < adjusted_level:
            html.append('<li><ul>')
            stack.append(len(stack) + 1)
        
        # Add the item
        html.append(f'<li><a href="#{heading["slug"]}">{heading["text"]}</a>')
        
        # Check if next heading is a child
        next_idx = headings.index(heading) + 1
        if next_idx < len(headings) and headings[next_idx]['level'] > level:
            html.append('<ul>')
            stack.append(len(stack) + 1)
        else:
            html.append('</li>')
    
    # Close remaining lists
    while len(stack) > 1:
        html.append('</ul></li>')
        stack.pop()
    
    html.append('</ul>')
    return '\n'.join(html)


def convert_markdown_to_html(content: str) -> str:
    """Convert Markdown content to HTML."""
    try:
        import markdown
        from markdown.extensions.codehilite import CodeHiliteExtension
        from markdown.extensions.fenced_code import FencedCodeExtension
        from markdown.extensions.tables import TableExtension
        from markdown.extensions.toc import TocExtension
        
        md = markdown.Markdown(extensions=[
            'fenced_code',
            'tables',
            'codehilite',
            TocExtension(slugify=slugify, toc_depth=6),
        ], extension_configs={
            'codehilite': {
                'css_class': 'codehilite',
                'guess_lang': False,
            }
        })
        return md.convert(content)
    except ImportError:
        print("Error: 'markdown' package is required. Install with: pip install markdown")
        sys.exit(1)


def generate_html(content: str, title: str, toc_html: str, show_draft: bool) -> str:
    """Generate the complete HTML document."""
    draft_banner = ''
    draft_styles = ''
    
    if show_draft:
        draft_banner = '''    <div class="draft-banner">
        <strong>DRAFT</strong> This specification is a working draft and may change at any time. 本规范为草稿版本，内容可能随时变更。
    </div>
'''
        draft_styles = '''        .sidebar {
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
        }'''
    
    return f'''<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{title}</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github.min.css">
    <style>
        :root {{
            --primary-color: #0066cc;
            --bg-color: #ffffff;
            --text-color: #333333;
            --code-bg: #f6f8fa;
            --border-color: #e1e4e8;
            --toc-width: 280px;
        }}
        * {{
            box-sizing: border-box;
        }}
        html {{
            scroll-behavior: smooth;
        }}
        body {{
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
            line-height: 1.6;
            color: var(--text-color);
            background-color: var(--bg-color);
            margin: 0;
            padding: 0;
        }}
        .container {{
            display: flex;
            max-width: 1400px;
            margin: 0 auto;
        }}
        .sidebar {{
            width: var(--toc-width);
            position: fixed;
            top: 0;
            left: max(0px, calc((100vw - 1400px) / 2));
            height: 100vh;
            overflow-y: auto;
            padding: 20px;
            background: #f8f9fa;
            border-right: 1px solid var(--border-color);
        }}
        .sidebar h2 {{
            font-size: 14px;
            text-transform: uppercase;
            color: #666;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 1px solid var(--border-color);
        }}
        .sidebar ul {{
            list-style: none;
            padding: 0;
            margin: 0;
        }}
        .sidebar li {{
            margin: 4px 0;
        }}
        .sidebar a {{
            color: #555;
            text-decoration: none;
            font-size: 13px;
            display: block;
            padding: 4px 8px;
            border-radius: 4px;
            transition: all 0.2s;
        }}
        .sidebar a:hover {{
            background: #e9ecef;
            color: var(--primary-color);
        }}
        .sidebar ul ul {{
            padding-left: 15px;
        }}
        .sidebar ul ul a {{
            font-size: 12px;
            color: #777;
        }}
        .content {{
            flex: 1;
            margin-left: var(--toc-width);
            padding: 40px 60px;
            max-width: calc(100% - var(--toc-width));
        }}
        h1, h2, h3, h4, h5, h6 {{
            margin-top: 24px;
            margin-bottom: 16px;
            font-weight: 600;
            line-height: 1.25;
        }}
        h1 {{
            font-size: 2em;
            padding-bottom: 0.3em;
            border-bottom: 1px solid var(--border-color);
        }}
        h2 {{
            font-size: 1.5em;
            padding-bottom: 0.3em;
            border-bottom: 1px solid var(--border-color);
        }}
        h3 {{ font-size: 1.25em; }}
        h4 {{ font-size: 1em; }}
        h5 {{ font-size: 0.875em; }}
        h6 {{ font-size: 0.85em; color: #6a737d; }}
        
        a {{
            color: var(--primary-color);
            text-decoration: none;
        }}
        a:hover {{
            text-decoration: underline;
        }}
        p {{
            margin-top: 0;
            margin-bottom: 16px;
        }}
        code {{
            font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace;
            font-size: 85%;
            background-color: var(--code-bg);
            padding: 0.2em 0.4em;
            border-radius: 6px;
        }}
        pre {{
            background-color: var(--code-bg);
            border-radius: 6px;
            padding: 16px;
            overflow: auto;
            font-size: 85%;
            line-height: 1.45;
            border: 1px solid var(--border-color);
        }}
        pre code {{
            background-color: transparent;
            padding: 0;
            border-radius: 0;
            font-size: 100%;
        }}
        table {{
            border-collapse: collapse;
            width: 100%;
            margin: 16px 0;
            overflow: auto;
        }}
        th, td {{
            border: 1px solid var(--border-color);
            padding: 8px 13px;
            text-align: left;
        }}
        th {{
            background-color: #f6f8fa;
            font-weight: 600;
        }}
        tr:nth-child(even) {{
            background-color: #f9f9f9;
        }}
        blockquote {{
            margin: 0;
            padding: 0 1em;
            color: #6a737d;
            border-left: 0.25em solid #dfe2e5;
        }}
        hr {{
            height: 0.25em;
            padding: 0;
            margin: 24px 0;
            background-color: var(--border-color);
            border: 0;
        }}
        ul, ol {{
            padding-left: 2em;
            margin-top: 0;
            margin-bottom: 16px;
        }}
        li {{
            margin: 4px 0;
        }}
        li > ul, li > ol {{
            margin-top: 4px;
            margin-bottom: 0;
        }}
        strong {{
            font-weight: 600;
        }}
        img {{
            max-width: 100%;
            box-sizing: content-box;
        }}
        .hljs {{
            background: var(--code-bg) !important;
        }}
        @media (max-width: 900px) {{
            .sidebar {{
                display: none;
            }}
            .content {{
                margin-left: 0;
                max-width: 100%;
                padding: 20px;
            }}
        }}
        /* Back to top button */
        .back-to-top {{
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
        }}
        .back-to-top.visible {{
            opacity: 1;
        }}
        .back-to-top:hover {{
            background: #0052a3;
        }}
        /* Draft banner */
        .draft-banner {{
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
        }}
        .draft-banner strong {{
            color: #5a4a06;
        }}
{draft_styles}
    </style>
</head>
<body>
{draft_banner}    <div class="container">
        <nav class="sidebar">
            <h2>Table of Contents</h2>
            <div class="toc">
{toc_html}
            </div>
        </nav>
        <main class="content">
            {content}
        </main>
    </div>
    <button class="back-to-top" onclick="window.scrollTo({{top: 0, behavior: 'smooth'}})">↑</button>
    
    <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/languages/xml.min.js"></script>
    <script>
        // Initialize highlight.js
        hljs.highlightAll();
        
        // Back to top button visibility
        window.addEventListener('scroll', function() {{
            const btn = document.querySelector('.back-to-top');
            if (window.scrollY > 300) {{
                btn.classList.add('visible');
            }} else {{
                btn.classList.remove('visible');
            }}
        }});
    </script>
</body>
</html>'''


def main():
    parser = argparse.ArgumentParser(
        description='Publish PAGX specification as HTML',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
    # Publish to default location (pagx/.spec_output/)
    python publish_spec.py

    # Publish as draft
    python publish_spec.py --draft

    # Publish to custom directory
    python publish_spec.py --output /path/to/output

    # Publish as draft to custom directory
    python publish_spec.py --draft --output /path/to/output
'''
    )
    parser.add_argument(
        '--output', '-o',
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help=f'Output directory (default: {DEFAULT_OUTPUT_DIR})'
    )
    parser.add_argument(
        '--draft', '-d',
        action='store_true',
        help='Show draft banner at the top of the page'
    )
    args = parser.parse_args()

    # Check if source file exists
    if not SPEC_FILE.exists():
        print(f"Error: Specification file not found: {SPEC_FILE}")
        sys.exit(1)

    # Read Markdown content
    print(f"Reading: {SPEC_FILE}")
    with open(SPEC_FILE, 'r', encoding='utf-8') as f:
        md_content = f.read()

    # Extract title from first heading
    title_match = re.match(r'^#\s+(.+)$', md_content, re.MULTILINE)
    title = title_match.group(1) if title_match else 'PAGX Format Specification'

    # Parse headings for TOC
    headings = parse_markdown_headings(md_content)

    # Generate TOC HTML
    toc_html = generate_toc_html(headings)

    # Convert Markdown to HTML
    html_content = convert_markdown_to_html(md_content)

    # Generate complete HTML document
    html = generate_html(html_content, title, toc_html, args.draft)

    # Create output directory
    args.output.mkdir(parents=True, exist_ok=True)

    # Write output file
    output_file = args.output / 'index.html'
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(html)

    print(f"Published to: {output_file}")
    if args.draft:
        print("  (with draft banner)")


if __name__ == '__main__':
    main()
