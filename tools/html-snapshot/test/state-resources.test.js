// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const { StateResources } = require('../dist/lib/state-resources');

test('a late body read cannot overwrite a newer response to the same URL', async () => {
  const cache = new StateResources('puppeteer');
  let finishOld;
  const response = body => ({request:()=>({resourceType:()=> 'image',url:()=> 'https://example.test/image'}),
    ok:()=>true,headers:()=>({'content-type':'image/png'}),buffer:()=>body});
  cache.onResponse(response(new Promise(resolve => { finishOld=resolve; })));
  cache.onResponse(response(Promise.resolve(Buffer.from('new'))));
  finishOld(Buffer.from('old'));
  const result = await cache.snapshot(1000);
  expect(result.get('https://example.test/image').buffer.toString()).toBe('new');
});
