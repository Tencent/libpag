'use strict';

const {
  isHttpUrl,
  fail,
  makeFail,
  parseNumber,
  errMessage,
  LOG_PREFIX,
} = require('../lib/common');
const { captureExit } = require('./helpers');

describe('isHttpUrl', () => {
  test.each([
    ['http://example.com', true],
    ['https://example.com/path?q=1', true],
    ['HTTP://EXAMPLE.COM', true],
    ['HTTPS://example.com', true],
    ['ftp://example.com', false],
    ['file:///tmp/page.html', false],
    ['/local/path.html', false],
    ['page.html', false],
    ['htttp://typo.com', false],
    ['', false],
  ])('isHttpUrl(%j) === %j', (input, expected) => {
    expect(isHttpUrl(input)).toBe(expected);
  });
});

describe('errMessage', () => {
  test('returns the message of a real Error', () => {
    expect(errMessage(new Error('boom'))).toBe('boom');
  });

  test('stringifies a thrown string', () => {
    expect(errMessage('plain string')).toBe('plain string');
  });

  test('stringifies undefined and null', () => {
    expect(errMessage(undefined)).toBe('undefined');
    expect(errMessage(null)).toBe('null');
  });

  test('falls back to String() when message is empty', () => {
    expect(errMessage({ message: '' })).toBe('[object Object]');
  });
});

describe('parseNumber', () => {
  test('parses a valid non-negative number', () => {
    expect(parseNumber('--wait-ms', '800')).toBe(800);
  });

  test('honours a custom min bound', () => {
    expect(parseNumber('--viewport-width', '1', { min: 1 })).toBe(1);
  });

  test('reports via the injected fail and rejects values below min', () => {
    const calls = [];
    const failFn = (msg) => { calls.push(msg); };
    parseNumber('--viewport-width', '0', { min: 1, fail: failFn });
    expect(calls).toHaveLength(1);
    expect(calls[0]).toMatch(/>= 1/);
  });

  test('rejects non-numeric input', () => {
    const calls = [];
    parseNumber('--wait-ms', 'abc', { fail: (m) => calls.push(m) });
    expect(calls[0]).toMatch(/non-negative/);
  });

  test('reports a missing argument', () => {
    const calls = [];
    parseNumber('--wait-ms', undefined, { fail: (m) => calls.push(m) });
    expect(calls[0]).toMatch(/requires a numeric argument/);
  });

  test('default fail path calls process.exit(2)', () => {
    const { exited, code, messages } = captureExit(() =>
      parseNumber('--wait-ms', 'nope'));
    expect(exited).toBe(true);
    expect(code).toBe(2);
    expect(messages[0]).toContain(LOG_PREFIX);
  });
});

describe('fail / makeFail', () => {
  test('fail logs with the standard prefix and exits 2', () => {
    const { exited, code, messages } = captureExit(() => fail('nope'));
    expect(exited).toBe(true);
    expect(code).toBe(2);
    expect(messages[0]).toBe(`${LOG_PREFIX}nope`);
  });

  test('makeFail uses the supplied prefix and default code', () => {
    const failWith = makeFail('eval');
    const { exited, code, messages } = captureExit(() => failWith('bad'));
    expect(exited).toBe(true);
    expect(code).toBe(2);
    expect(messages[0]).toBe('eval: bad');
  });

  test('makeFail honours an explicit exit code', () => {
    const failWith = makeFail('eval');
    const { code } = captureExit(() => failWith('bad', 7));
    expect(code).toBe(7);
  });
});
