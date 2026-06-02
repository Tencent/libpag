'use strict';

const {
  HttpError,
  safeQueryParam,
  setNumberParam,
  setStringParam,
  setBoolParam,
  wantsJsonResponse,
} = require('../dist/lib/http-utils');

describe('HttpError', () => {
  test('carries a status code and message', () => {
    const err = new HttpError(400, 'bad request');
    expect(err).toBeInstanceOf(Error);
    expect(err.status).toBe(400);
    expect(err.message).toBe('bad request');
  });
});

describe('safeQueryParam', () => {
  test('extracts a present query parameter', () => {
    expect(safeQueryParam({ url: '/snapshot?url=https://x.com' }, 'url'))
      .toBe('https://x.com');
  });

  test('returns null when the parameter is absent', () => {
    expect(safeQueryParam({ url: '/snapshot?foo=1' }, 'url')).toBeNull();
  });

  test('returns null for an unparseable URL', () => {
    expect(safeQueryParam({ url: '%' }, 'url')).toBeNull();
  });
});

describe('setNumberParam', () => {
  test('assigns the coerced number when present', () => {
    const target = {};
    setNumberParam(new URLSearchParams('w=800'), target, 'w');
    expect(target.w).toBe(800);
  });

  test('leaves the target untouched when absent', () => {
    const target = {};
    setNumberParam(new URLSearchParams(''), target, 'w');
    expect('w' in target).toBe(false);
  });

  test('preserves NaN for non-numeric input', () => {
    const target = {};
    setNumberParam(new URLSearchParams('w=abc'), target, 'w');
    expect(Number.isNaN(target.w)).toBe(true);
  });
});

describe('setStringParam', () => {
  test('assigns the raw string when present', () => {
    const target = {};
    setStringParam(new URLSearchParams('sel=%23root'), target, 'sel');
    expect(target.sel).toBe('#root');
  });

  test('leaves the target untouched when absent', () => {
    const target = {};
    setStringParam(new URLSearchParams(''), target, 'sel');
    expect('sel' in target).toBe(false);
  });
});

describe('setBoolParam', () => {
  test.each([
    ['flag=true', true],
    ['flag=1', true],
    ['flag=false', false],
    ['flag=0', false],
  ])('coerces %s', (query, expected) => {
    const target = {};
    setBoolParam(new URLSearchParams(query), target, 'flag');
    expect(target.flag).toBe(expected);
  });

  test('leaves the target untouched when absent', () => {
    const target = {};
    setBoolParam(new URLSearchParams(''), target, 'flag');
    expect('flag' in target).toBe(false);
  });

  test('throws HttpError 400 on an invalid value', () => {
    const target = {};
    expect(() => setBoolParam(new URLSearchParams('flag=maybe'), target, 'flag'))
      .toThrow(HttpError);
    try {
      setBoolParam(new URLSearchParams('flag=maybe'), target, 'flag');
    } catch (err) {
      expect(err.status).toBe(400);
      expect(err.message).toMatch(/must be true\|false\|1\|0/);
    }
  });
});

describe('wantsJsonResponse', () => {
  const req = (accept) => ({ headers: accept === undefined ? {} : { accept } });

  test('true when JSON is explicitly requested without text/html', () => {
    expect(wantsJsonResponse(req('application/json'))).toBe(true);
  });

  test('false when text/html is also acceptable (browser default)', () => {
    expect(wantsJsonResponse(req('text/html, application/xhtml+xml, application/json, */*')))
      .toBe(false);
  });

  test('false when JSON is not requested at all', () => {
    expect(wantsJsonResponse(req('text/plain'))).toBe(false);
  });

  test('false when there is no Accept header', () => {
    expect(wantsJsonResponse(req())).toBe(false);
  });
});
