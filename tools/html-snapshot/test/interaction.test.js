// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const { parseScenario } = require('../dist/lib/interaction');
const capture = { type:'capture', name:'state' };
test.each([
  [{ version:2, steps:[capture] }, /version/],
  [{ version:1, typo:true, steps:[capture] }, /unknown field/],
  [{ version:1, steps:[capture, capture] }, /duplicate checkpoint/],
  [{ version:1, steps:[{ type:'click' }, capture] }, /target/],
  [{ version:1, steps:[{ type:'click', target:{css:'a',testId:'a'} }, capture] }, /exactly one/],
  [{ version:1, steps:[{ type:'fill', target:{css:'input'}, value:1 }, capture] }, /value/],
  [{ version:1, steps:[{ type:'waitFor', target:{css:'a'}, state:'visible', text:'ok' }, capture] }, /exactly one/],
  [{ version:1, steps:[{ type:'capture', name:'../escape' }] }, /checkpoint name/],
  [{ version:1, timeoutMs:0, steps:[capture] }, /timeoutMs/],
  [{ version:1, steps:[{ type:'evaluate', script:'alert(1)' }, capture] }, /unsupported action/],
])('validates scenarios before running any action (%j)', (value, message) => {
  expect(() => parseScenario(value)).toThrow(message);
});
test('normalizes defaults without modifying caller data', () => {
  const input = { version:1, steps:[capture] };
  expect(parseScenario(input)).toMatchObject({ viewport:{width:1400,height:900}, stableMs:150 });
  expect(input).toEqual({ version:1, steps:[capture] });
});
