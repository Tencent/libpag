// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import * as http from 'node:http';
import * as net from 'node:net';

export function websiteOrigins(values: string[] = []): string[] {
  return [...new Set(values.map(value => {
    const url = new URL(value);
    if (!['http:', 'https:'].includes(url.protocol) || url.username || url.password ||
        url.pathname !== '/' || url.search || url.hash) throw new Error(`Expected an HTTP(S) origin: ${value}`);
    return url.origin;
  }))].sort();
}

/** Browser-wide proxy: workers and WebSockets obey the same allowlist as page requests.
 * This is a resource policy; production workers still need OS/container isolation.
 */
export async function startWebsiteProxy(origins: string[]): Promise<{ url: string; close(): Promise<void> }> {
  const allowed = new Set(websiteOrigins(origins));
  const sockets = new Set<net.Socket>();
  const track = (socket: net.Socket) => { sockets.add(socket); socket.once('close', () => sockets.delete(socket)); return socket; };
  const server = http.createServer((req, res) => {
    let url: URL;
    try { url = new URL(req.url!); }
    catch (_) { res.writeHead(400); res.end(); return; }
    if (url.protocol !== 'http:' || !allowed.has(url.origin) || url.username || url.password) {
      res.writeHead(403); res.end('Website resource origin is not allowed'); return;
    }
    const headers: http.OutgoingHttpHeaders = { ...req.headers, host: url.host };
    delete headers['proxy-authorization']; delete headers['proxy-connection'];
    const upstream = http.request(url, { method: req.method, headers, agent: false }, response => {
      res.writeHead(response.statusCode || 502, response.headers);
      response.on('error', () => res.destroy());
      response.pipe(res);
    });
    upstream.on('socket', track);
    upstream.on('error', () => { if (!res.headersSent) res.writeHead(502); res.end(); });
    req.on('aborted', () => upstream.destroy());
    res.on('close', () => upstream.destroy());
    req.pipe(upstream);
  });
  server.on('connection', track);
  server.on('connect', (req, socket, head) => {
    let url: URL;
    try { url = new URL('https://' + req.url); }
    catch (_) { socket.destroy(); return; }
    if (!allowed.has(url.origin) || url.username || url.password || url.pathname !== '/') {
      socket.end('HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n'); return;
    }
    const upstream = track(net.connect({ host: url.hostname.replace(/^\[|\]$/g, ''), port: Number(url.port || 443) }));
    upstream.once('connect', () => {
      socket.write('HTTP/1.1 200 Connection Established\r\n\r\n');
      if (head.length) upstream.write(head);
      socket.pipe(upstream); upstream.pipe(socket);
    });
    upstream.on('error', () => socket.destroy()); socket.on('error', () => upstream.destroy());
    socket.on('close', () => upstream.destroy()); upstream.on('close', () => socket.destroy());
  });
  // Plain WebSocket upgrades use the same origin restriction. HTTPS WebSockets use CONNECT.
  server.on('upgrade', (req, socket, head) => {
    let url: URL;
    try { url = new URL(req.url!); if (url.protocol === 'ws:') url.protocol = 'http:'; }
    catch (_) { socket.destroy(); return; }
    if (url.protocol !== 'http:' || !allowed.has(url.origin) || url.username || url.password) { socket.destroy(); return; }
    const upstream = track(net.connect({ host: url.hostname.replace(/^\[|\]$/g, ''), port: Number(url.port || 80) }));
    upstream.once('connect', () => {
      const headers = Object.entries({ ...req.headers, host: url.host })
        .filter(([name]) => !name.startsWith('proxy-')).map(([name, value]) => `${name}: ${value}`).join('\r\n');
      upstream.write(`${req.method} ${url.pathname}${url.search} HTTP/1.1\r\n${headers}\r\n\r\n`);
      if (head.length) upstream.write(head);
      socket.pipe(upstream); upstream.pipe(socket);
    });
    upstream.on('error', () => socket.destroy()); socket.on('error', () => upstream.destroy());
    socket.on('close', () => upstream.destroy()); upstream.on('close', () => socket.destroy());
  });
  await new Promise<void>((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', () => { server.off('error', reject); resolve(); });
  });
  let closing: Promise<void> | undefined;
  return {
    url: `http://127.0.0.1:${(server.address() as net.AddressInfo).port}`,
    close: () => closing ??= new Promise<void>((resolve, reject) => {
      server.close(error => error ? reject(error) : resolve());
      for (const socket of sockets) socket.destroy();
    }),
  };
}
