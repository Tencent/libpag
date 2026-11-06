// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const http = require('http');
const net = require('net');
const { startWebsiteProxy, websiteOrigins } = require('../dist/lib/website-proxy');
const servers = [], proxies = [], sockets = new Set();
async function listen(server) {
  servers.push(server);
  server.on('connection', socket => { sockets.add(socket); socket.once('close', () => sockets.delete(socket)); });
  await new Promise((resolve, reject) => { server.once('error', reject); server.listen(0, '127.0.0.1', resolve); });
  return server.address().port;
}
async function proxy(origins) {
  const value = await startWebsiteProxy(origins); proxies.push(value); return value;
}
afterEach(async () => {
  for (const value of proxies.splice(0)) await value.close();
  for (const socket of sockets) socket.destroy();
  for (const server of servers.splice(0)) await new Promise(resolve => server.close(resolve));
});
function exchange(url, request, complete = () => false) {
  return new Promise((resolve, reject) => {
    let received = '';
    const socket = net.connect(new URL(url).port, '127.0.0.1', () => socket.write(request));
    socket.on('error', reject);
    socket.on('data', bytes => {
      received += bytes;
      if (complete(received)) { socket.destroy(); resolve(received); }
    });
    socket.on('close', () => resolve(received));
    socket.setTimeout(2000, () => { socket.destroy(); reject(new Error('proxy exchange timed out')); });
  });
}
function get(url, target, headers = {}) {
  return new Promise((resolve, reject) => {
    http.get(url, { path: target, headers }, response => {
      let body = ''; response.on('data', bytes => { body += bytes; });
      response.on('error', reject);
      response.on('end', () => resolve({ status: response.statusCode, body }));
    }).on('error', reject);
  });
}

test('normalizes origins and rejects credentials, paths, queries and unsupported schemes', () => {
  expect(websiteOrigins(['HTTP://Example.test:80', 'http://example.test/', 'https://example.test:443/']))
    .toEqual(['http://example.test', 'https://example.test']);
  for (const value of ['ftp://example.test', 'http://user@example.test', 'https://example.test/path',
    'http://example.test/?query', 'http://example.test/#fragment', 'not a URL']) {
    expect(() => websiteOrigins([value])).toThrow();
  }
});

test('forwards request bodies while stripping proxy credentials and replacing Host', async () => {
  let received;
  const port = await listen(http.createServer(async (req, res) => {
    const chunks = []; for await (const chunk of req) chunks.push(chunk);
    received = { method: req.method, headers: req.headers, body: Buffer.concat(chunks).toString() };
    res.writeHead(201); res.end('created');
  }));
  const origin = `http://127.0.0.1:${port}`, forward = await proxy([origin]);
  const response = await exchange(forward.url, `POST ${origin}/save?x=1 HTTP/1.1\r\nHost: wrong.example\r\n` +
    'Proxy-Authorization: secret\r\nProxy-Connection: keep-alive\r\nContent-Length: 4\r\nConnection: close\r\n\r\nbody');
  expect(response).toContain('201 Created');
  expect(received).toMatchObject({ method: 'POST', headers: { host: `127.0.0.1:${port}` }, body: 'body' });
  expect(received.headers).not.toHaveProperty('proxy-authorization');
  expect(received.headers).not.toHaveProperty('proxy-connection');
  expect((await get(forward.url, '/relative')).status).toBe(400);
  for (const target of [`https://127.0.0.1:${port}/`, `http://user:password@127.0.0.1:${port}/`]) {
    expect((await get(forward.url, target)).status).toBe(403);
  }
});

test('CONNECT forwards early tunnel bytes in both directions and closes idempotently', async () => {
  const port = await listen(net.createServer(socket => socket.on('data', bytes => socket.write(Buffer.concat([Buffer.from('echo:'), bytes])))));
  const forward = await proxy([`https://127.0.0.1:${port}`]);
  const response = await exchange(forward.url,
    `CONNECT 127.0.0.1:${port} HTTP/1.1\r\nHost: 127.0.0.1:${port}\r\n\r\nearly-data`, text => text.includes('echo:early-data'));
  expect(response).toContain('200 Connection Established');
  expect(response).toContain('echo:early-data');
  const closing = forward.close(); expect(forward.close()).toBe(closing); await closing;
});

test.each(['invalid[', 'user:pass@127.0.0.1:443', '127.0.0.1:443/path', 'blocked.test:443'])
  ('rejects invalid or forbidden CONNECT destinations: %s', async target => {
    const forward = await proxy(['https://127.0.0.1']);
    const response = await exchange(forward.url, `CONNECT ${target} HTTP/1.1\r\nHost: localhost\r\n\r\n`);
    if (target === 'invalid[') expect(response).toBe('');
    else expect(response).toContain('403 Forbidden');
  });

test.each(['ws:', 'http:'])('%s upgrades preserve the path and early bytes without proxy headers', async protocol => {
  let received;
  const upstream = http.createServer();
  upstream.on('upgrade', (req, socket, head) => {
    received = { url: req.url, headers: req.headers };
    socket.write('HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n\r\n');
    if (head.length) socket.write(head);
    socket.on('data', bytes => socket.write(bytes));
  });
  const port = await listen(upstream), forward = await proxy([`http://127.0.0.1:${port}`]);
  const response = await exchange(forward.url, `GET ${protocol}//127.0.0.1:${port}/chat?channel=a HTTP/1.1\r\n` +
    'Host: wrong.example\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nProxy-Authorization: secret\r\n\r\nfirst-frame',
  text => text.includes('first-frame'));
  expect(response).toContain('101 Switching Protocols');
  expect(received).toMatchObject({ url: '/chat?channel=a', headers: { host: `127.0.0.1:${port}` } });
  expect(received.headers).not.toHaveProperty('proxy-authorization');
});

test.each(['/relative', 'ws://blocked.test/', 'ftp://allowed.test/', 'ws://user:pass@allowed.test/'])
  ('rejects invalid or forbidden upgrades: %s', async target => {
    const forward = await proxy(['http://allowed.test']);
    expect(await exchange(forward.url, `GET ${target} HTTP/1.1\r\nHost: allowed.test\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n\r\n`)).toBe('');
  });

test('unreachable upstreams produce 502 for HTTP and close tunnels and upgrades', async () => {
  const closed = net.createServer(), port = await listen(closed);
  await new Promise(resolve => closed.close(resolve));
  const host = `127.0.0.1:${port}`, forward = await proxy([`http://${host}`, `https://${host}`]);
  expect((await get(forward.url, `http://${host}/`)).status).toBe(502);
  expect(await exchange(forward.url, `CONNECT ${host} HTTP/1.1\r\nHost: ${host}\r\n\r\n`)).toBe('');
  expect(await exchange(forward.url, `GET ws://${host}/ HTTP/1.1\r\nHost: ${host}\r\nConnection: Upgrade\r\nUpgrade: websocket\r\n\r\n`)).toBe('');
});
