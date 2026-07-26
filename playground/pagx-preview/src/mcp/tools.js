/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import fs from 'fs';
import path from 'path';

export const UI_MIME = 'text/html;profile=mcp-app';
export const UI_URI = 'ui://pagx-preview/main';

// After this many consecutive get_document calls with no summary, assume the host is not
// rendering the inline widget and fall back to returning the browser-openable webview url so the
// caller can open the preview there instead of polling a widget that will never load.
const MAX_DOCUMENT_QUERY_FAILURES = 3;

// Tool definitions. Kept as plain objects so both listTools and callTool can reference the same
// schema without duplicating field names / descriptions.
const TOOLS = [
  {
    name: 'preview_pagx',
    description:
      'Load a pagx file for preview. Renders an interactive widget in the conversation showing the pagx animation, and returns a url that can be opened in a browser / webview. Returns session info including sessionId for use with reload_file / get_document.',
    inputSchema: {
      type: 'object',
      properties: {
        file: { type: 'string', description: 'Absolute path to the .pagx file to preview.' },
      },
      required: ['file'],
    },
    // MCP Apps: associate this tool with the UI resource so the host renders the widget inline.
    // Both nested and flat keys are required — Claude Desktop reads the flat key "ui/resourceUri"
    // to decide whether to mount an iframe. The nested form is for other hosts.
    _meta: { ui: { resourceUri: UI_URI }, 'ui/resourceUri': UI_URI },
  },
  {
    name: 'reload_file',
    description:
      'Force a full reload of the pagx file from disk. The preview widget automatically reloads on file change, but this tool can trigger a manual reload if needed.',
    inputSchema: {
      type: 'object',
      properties: {
        sessionId: { type: 'string', description: 'Session ID returned by preview_pagx.' },
      },
      required: ['sessionId'],
    },
  },
  {
    name: 'get_document',
    description:
      'Get a summary of the loaded pagx document: dimensions and animation duration. The summary is uploaded by the client after load; it may be null if the client has not finished loading yet. If it keeps returning "not loaded" for several consecutive calls, the inline widget is likely not rendering in this host and the result will switch to a fallback that returns a browser-openable url (fallbackToWebview: true) — open that url in a webview / browser instead of polling further.',
    inputSchema: {
      type: 'object',
      properties: {
        sessionId: { type: 'string', description: 'Session ID returned by preview_pagx.' },
      },
      required: ['sessionId'],
    },
  },
];

/**
 * Builds the listTools / callTool handlers for the MCP server.
 * @returns {{ listTools: Function, callTool: Function }}
 */
export function buildToolHandlers({ sessions, createOrGetSession, getServerBaseUrl }) {
  return {
    async listTools() {
      return { tools: TOOLS };
    },

    async callTool(req) {
      const { name, arguments: args } = req.params;

      if (name === 'preview_pagx') {
        return handlePreviewPagx(args, { createOrGetSession, getServerBaseUrl });
      }
      if (name === 'reload_file') {
        return handleReloadFile(args, { sessions });
      }
      if (name === 'get_document') {
        return handleGetDocument(args, { sessions, getServerBaseUrl });
      }
      return {
        content: [{ type: 'text', text: `unknown tool: ${name}` }],
        isError: true,
      };
    },
  };
}

function handlePreviewPagx(args, { createOrGetSession, getServerBaseUrl }) {
  const file = args?.file;
  if (!file || !fs.existsSync(file)) {
    return {
      content: [{ type: 'text', text: `file not found: ${file}` }],
      isError: true,
    };
  }
  const { session, reused } = createOrGetSession(path.resolve(file));
  const baseUrl = getServerBaseUrl ? getServerBaseUrl() : '';
  const url = baseUrl ? `${baseUrl}/session/${session.id}/` : `/session/${session.id}/`;
  const summary = session.documentSummary;
  // The summary is uploaded by the client (widget or browser tab) only after it finishes
  // rendering, so on the first preview_pagx call it is usually still null. Report "rendering"
  // instead of a misleading "0 nodes, 0.0s"; get_document can be polled once the widget appears.
  const stats = summary
    ? `${summary.nodeCount} nodes, ${(summary.duration / 1000000).toFixed(1)}s duration`
    : 'rendering (document summary available after the widget finishes loading)';
  return {
    content: [
      {
        type: 'text',
        text: `Previewing ${path.basename(file)}${reused ? ' (reusing existing session)' : ''} — ${stats}.\n\nIf the inline preview does not appear above, you can:\n1. Open in browser: ${url}\n2. Open the URL in IDE webview for in-editor preview`,
      },
    ],
    structuredContent: {
      sessionId: session.id,
      file: session.entryFile,
      fileName: path.basename(session.entryFile),
      url,
      nodeCount: summary?.nodeCount ?? 0,
      duration: summary?.duration ?? 0,
      reused,
    },
  };
}

function handleReloadFile(args, { sessions }) {
  const { sessionId } = args ?? {};
  const session = sessions.get(sessionId);
  if (!session) {
    return {
      content: [{ type: 'text', text: `session not found: ${sessionId}` }],
      isError: true,
    };
  }
  session.emit({ type: 'reload', file: session.entryFile, event: 'mcp-reload' });
  return {
    content: [{ type: 'text', text: `reload triggered for session ${sessionId}` }],
    structuredContent: { sessionId, reloaded: true },
  };
}

function handleGetDocument(args, { sessions, getServerBaseUrl }) {
  const { sessionId } = args ?? {};
  const session = sessions.get(sessionId);
  if (!session) {
    return {
      content: [{ type: 'text', text: `session not found: ${sessionId}` }],
      isError: true,
    };
  }
  const summary = session.documentSummary;
  if (!summary) {
    // The inline widget uploads its summary only after it renders. If the host does not render
    // the widget at all, the summary never arrives and every poll returns null. Count consecutive
    // misses; once we hit the threshold, stop reporting an error and instead hand back the
    // browser-openable webview url so the caller opens the preview there rather than polling a
    // widget that will never load.
    session.documentQueryFailures += 1;
    if (session.documentQueryFailures >= MAX_DOCUMENT_QUERY_FAILURES) {
      const baseUrl = getServerBaseUrl ? getServerBaseUrl() : '';
      const url = baseUrl ? `${baseUrl}/session/${session.id}/` : `/session/${session.id}/`;
      return {
        content: [
          {
            type: 'text',
            text: `The inline widget has not reported a document after ${session.documentQueryFailures} consecutive attempts, so it is likely not rendering in this host. Open the preview in a webview / browser instead: ${url}`,
          },
        ],
        structuredContent: {
          sessionId: session.id,
          loaded: false,
          attempts: session.documentQueryFailures,
          fallbackToWebview: true,
          url,
        },
      };
    }
    return {
      content: [{ type: 'text', text: `document not loaded yet for session ${sessionId}` }],
      isError: true,
    };
  }
  session.documentQueryFailures = 0;
  return {
    content: [
      {
        type: 'text',
        text: `${summary.nodeCount} nodes, ${summary.width}x${summary.height}, ${(summary.duration / 1000000).toFixed(1)}s`,
      },
    ],
    structuredContent: summary,
  };
}

/**
 * Builds the listResources / readResource handlers for the MCP server.
 * @returns {{ listResources: Function, readResource: Function }}
 */
export function buildResourceHandlers({ staticDir, getServerBaseUrl }) {
  return {
    async listResources() {
      return {
        resources: [
          {
            uri: UI_URI,
            name: 'pagx-preview-widget',
            mimeType: UI_MIME,
            description: 'Interactive pagx preview widget',
          },
        ],
      };
    },

    async readResource(req) {
      if (req.params.uri !== UI_URI) {
        return { contents: [] };
      }
      const widgetPath = path.join(staticDir, 'mcp-widget.html');
      let html;
      if (fs.existsSync(widgetPath)) {
        html = fs.readFileSync(widgetPath, 'utf8');
      } else {
        html = '<html><body>pagx-preview widget not found. Run npm run prebuild.</body></html>';
      }
      // Inline the pre-built widget bundle (app-with-deps + pagx-player + mcp-widget merged
      // by esbuild into one minified file). This avoids external <script src="..."> which
      // Claude Desktop's sandbox blocks. Only WASM viewer + fonts + pagx bytes are fetched
      // at runtime via absolute URLs injected as __SERVER_BASE__.
      const baseUrl = getServerBaseUrl ? getServerBaseUrl() : '';
      const connectDomains = baseUrl ? [new URL(baseUrl).origin] : [];
      // Read the pre-built bundle and inject via a self-decoding <script>. We base64-encode
      // the JS to avoid any characters in the bundle (backticks, </script>, quotes, etc.) from
      // breaking `document.write()` or the <script> tag parser.
      const bundlePath = path.join(staticDir, 'mcp-widget.bundle.js');
      let bundleJs = fs.readFileSync(bundlePath, 'utf8');
      const baseUrlDecl = `var __SERVER_BASE__=${JSON.stringify(baseUrl || '')};`;
      bundleJs = baseUrlDecl + bundleJs;
      const b64 = Buffer.from(bundleJs).toString('base64');
      // Use a classic script that decodes and creates a blob URL module
      const loader = `<script>` +
        `(function(){` +
        `var s=atob("${b64}");` +
        `var b=new Blob([s],{type:"text/javascript"});` +
        `var u=URL.createObjectURL(b);` +
        `var el=document.createElement("script");` +
        `el.type="module";el.src=u;` +
        `document.head.appendChild(el);` +
        `})()` +
        `</script>`;
      html = html.replace(
        /<script\s+type="module"\s+src="\/static\/mcp-widget\.js"\s*><\/script>/,
        loader
      );
      // Remove meta CSP — let host control it
      html = html.replace(/<meta http-equiv="Content-Security-Policy"[\s\S]*?>/m, '');
      html = html.replace(/\s*<!-- CSP is controlled[\s\S]*?-->\s*/m, '\n  ');
      return {
        contents: [
          {
            uri: UI_URI,
            mimeType: UI_MIME,
            text: html,
            _meta: connectDomains.length ? {
              ui: {
                csp: {
                  resourceDomains: connectDomains,
                  connectDomains,
                  baseUriDomains: connectDomains,
                },
              },
            } : undefined,
          },
        ],
      };
    },
  };
}
