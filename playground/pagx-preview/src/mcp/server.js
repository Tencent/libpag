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

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StreamableHTTPServerTransport } from '@modelcontextprotocol/sdk/server/streamableHttp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
  ListResourcesRequestSchema,
  ReadResourceRequestSchema,
} from '@modelcontextprotocol/sdk/types.js';
import { buildToolHandlers, buildResourceHandlers } from './tools.js';

/**
 * Builds a configured MCP Server instance with all pagx-preview tool / resource handlers
 * registered. The handlers are plain closures over the shared session maps, so the same builder
 * serves both the HTTP (per-request) and stdio (long-lived) transports.
 *
 * @param {object} opts
 * @param {Map} opts.sessions - live PreviewSession map keyed by session id.
 * @param {Function} opts.createOrGetSession - creates or reuses a session for a file path.
 * @param {string} opts.staticDir - directory holding mcp-widget.html and static assets.
 * @param {Function} [opts.getServerBaseUrl] - returns the preview server's base URL (used to
 *   rewrite widget resource URLs and to build the browser-openable session url).
 * @returns {Server} a ready-to-connect MCP Server.
 */
export function createMcpServer(opts) {
  const { sessions, createOrGetSession, staticDir, getServerBaseUrl } = opts;
  const server = new Server(
    { name: 'pagx-preview', version: '0.1.0' },
    { capabilities: { tools: {}, resources: {} } },
  );

  const handlers = buildToolHandlers({ sessions, createOrGetSession, getServerBaseUrl });
  const resourceHandlers = buildResourceHandlers({ staticDir, getServerBaseUrl });

  server.setRequestHandler(ListToolsRequestSchema, handlers.listTools);
  server.setRequestHandler(CallToolRequestSchema, handlers.callTool);
  server.setRequestHandler(ListResourcesRequestSchema, resourceHandlers.listResources);
  server.setRequestHandler(ReadResourceRequestSchema, resourceHandlers.readResource);
  return server;
}

/**
 * Connects a single long-lived MCP Server to a stdio transport. This is the self-bootstrapping
 * entry point: an MCP client (CodeBuddy) spawns `pagx-preview --mcp` on demand and speaks MCP over
 * stdin/stdout, so the user never has to start a server manually. The caller owns the in-process
 * preview HTTP server whose session maps are shared here.
 *
 * @returns {Promise<Server>} the connected Server (its `onclose` fires when stdin closes).
 */
export async function connectStdioMcpServer(opts) {
  const server = createMcpServer(opts);
  const transport = new StdioServerTransport();
  await server.connect(transport);
  return server;
}

/**
 * Mounts the MCP server onto the given express app. The MCP server shares the same process,
 * port, and session map as the rest of pagx-preview so that tool calls can directly access
 * PreviewSession objects without a network round-trip.
 *
 * The endpoint is POST /mcp (Streamable HTTP transport). CodeBuddy connects by adding:
 *   { "mcpServers": { "pagx-preview": { "type": "http", "url": "http://127.0.0.1:<port>/mcp" } } }
 * to its mcp.json. Use `pagx-preview --port 7300 <file>` to pin a port for MCP config stability.
 *
 * Each POST /mcp request creates a fresh Server + Transport pair. The MCP SDK's Server.connect()
 * binds the Server to a single Transport for its lifetime, so reusing a Server across requests
 * triggers "Already connected to a transport". Creating per-request instances is cheap (handlers
 * are plain closures over the shared session map) and keeps the stateless request model clean.
 */
export function mountMcpServer(app, opts) {
  app.post('/mcp', async (req, res) => {
    const transport = new StreamableHTTPServerTransport({ sessionIdGenerator: undefined });
    const server = createMcpServer(opts);

    try {
      await server.connect(transport);
      // The app-level express.json() middleware has already consumed the request stream, so the
      // transport cannot re-read it. Pass the parsed body explicitly; otherwise handleRequest
      // reads an empty stream and reports "Parse error: Invalid JSON".
      await transport.handleRequest(req, res, req.body);
    } catch (err) {
      if (!res.headersSent) {
        res.status(500).json({ error: err.message });
      }
    } finally {
      // Close the Server so its internal transport reference is released. The Transport itself
      // is per-request and will be GC'd; the Server's handler closures only reference the
      // shared session maps, not per-request state, so no data leaks.
      try {
        await server.close();
      } catch (_) {
        // ignore
      }
    }
  });

  // GET/DELETE are not supported in stateless mode: there is no long-lived session to attach a
  // server-initiated SSE stream to, and every POST already spins up its own transport. Returning
  // a JSON-RPC "Method not allowed" here matches the official stateless server example, so an MCP
  // client that probes the legacy GET-SSE channel backs off cleanly instead of retrying.
  const methodNotAllowed = (req, res) => {
    res.status(405).json({
      jsonrpc: '2.0',
      error: { code: -32000, message: 'Method not allowed.' },
      id: null,
    });
  };
  app.get('/mcp', methodNotAllowed);
  app.delete('/mcp', methodNotAllowed);
}
