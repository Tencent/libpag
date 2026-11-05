// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const http = require('http');
const net = require('net');
const { startWebsiteProxy } = require('../dist/lib/website-proxy');
test('forwards only allowed HTTP origins and closes connections', async () => {
  let hits = 0;
  const upstream = http.createServer((req, res) => { hits++; res.end(req.url); });
  await new Promise(resolve => upstream.listen(0, '127.0.0.1', resolve));
  const origin = `http://127.0.0.1:${upstream.address().port}`;
  const proxy = await startWebsiteProxy([origin]);
  const request = target => new Promise((resolve, reject) => {
    http.get(proxy.url, { path: target }, res => {
      let body = ''; res.on('data', bytes => body += bytes);
      res.on('end', () => resolve({ status: res.statusCode, body }));
    }).on('error', reject);
  });
  try {
    expect(await request(origin + '/a?x=1')).toEqual({ status: 200, body: '/a?x=1' });
    expect((await request('http://localhost:' + upstream.address().port + '/')).status).toBe(403);
    expect(hits).toBe(1);
    const tunnel = await new Promise((resolve, reject) => {
      const socket = net.connect(new URL(proxy.url).port, '127.0.0.1', () => {
        socket.write('CONNECT blocked.example:443 HTTP/1.1\r\nHost: blocked.example:443\r\n\r\n');
      });
      socket.on('error', reject); socket.once('data', bytes => { resolve(bytes.toString()); socket.destroy(); });
    });
    expect(tunnel).toContain('403');
  } finally { await proxy.close(); await new Promise(resolve => upstream.close(resolve)); }
});
