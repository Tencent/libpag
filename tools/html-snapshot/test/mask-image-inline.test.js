'use strict';

// `inlineMaskImages` normally runs inside `page.evaluate` against a real DOM:
// it walks every element, reads `getComputedStyle(el).mask-image`, and rewrites
// a remote `url(https://…)` to a `data:` URI. A browser resolves such a url
// over the network, so the affected layer still renders when the snapshot is
// opened directly — but the importer hands the url to `ImageCodec::MakeFrom`,
// which cannot fetch, so it silently drops the whole mask layer and the masked
// element paints its unmasked fill instead (a chevron icon collapses to a solid
// rectangle). The function is plain DOM/fetch code, so we drive it in node
// behind minimal `document` / `getComputedStyle` / `fetch` / `FileReader`
// stubs, the same way icon-font-browser.test.js exercises the browser helpers.

const { inlineMaskImages } = require('../dist/lib/browser-snapshot');

class FakeElement {
  constructor(maskImage) {
    this.style = {};
    this.__maskImage = maskImage;
  }
}

const REMOTE = 'https://developer.mozilla.org/static/client/chevron-down.svg';
const DATA_URI = 'data:image/svg+xml;base64,PHN2ZyB4bWxucz0i';

let savedGlobals;
let warnSpy;

beforeEach(() => {
  savedGlobals = {
    document: global.document,
    getComputedStyle: global.getComputedStyle,
    fetch: global.fetch,
    FileReader: global.FileReader,
  };
  warnSpy = jest.spyOn(console, 'warn').mockImplementation(() => {});
});

afterEach(() => {
  global.document = savedGlobals.document;
  global.getComputedStyle = savedGlobals.getComputedStyle;
  global.fetch = savedGlobals.fetch;
  global.FileReader = savedGlobals.FileReader;
  warnSpy.mockRestore();
});

// Minimal FileReader: hands back whatever `__dataUri` the stubbed blob carries,
// asynchronously, mirroring the real readAsDataURL contract this pass relies on.
class FakeFileReader {
  readAsDataURL(blob) {
    setTimeout(() => {
      this.result = blob.__dataUri;
      this.onload();
    }, 0);
  }
}

function runOn(elements, fetchImpl) {
  global.document = { querySelectorAll: () => elements };
  global.getComputedStyle = (el) => ({
    getPropertyValue: (k) => (k === 'mask-image' ? el.__maskImage || '' : ''),
  });
  global.FileReader = FakeFileReader;
  global.fetch = fetchImpl || (async () => ({
    ok: true,
    blob: async () => ({ __dataUri: DATA_URI }),
  }));
  return inlineMaskImages();
}

describe('inlineMaskImages', () => {
  test('rewrites a remote mask url to a data URI, single-quoted', async () => {
    const el = new FakeElement(`url("${REMOTE}")`);
    await runOn([el]);

    expect(el.style.maskImage).toBe(`url('${DATA_URI}')`);
    // The value lands inside the double-quoted style="…" attribute the walker
    // writes, where a double-quoted url() would close the attribute early.
    expect(el.style.maskImage).not.toContain('"');
  });

  test('keeps the other layers of a multi-layer mask value', async () => {
    const el = new FakeElement(`url("${REMOTE}"), linear-gradient(#000, #000)`);
    await runOn([el]);

    expect(el.style.maskImage).toBe(`url('${DATA_URI}'), linear-gradient(#000, #000)`);
  });

  test('leaves gradients, data: and file: masks untouched', async () => {
    const gradient = new FakeElement('linear-gradient(90deg, rgba(0, 0, 0, 0), rgb(0, 0, 0) 75%)');
    const inline = new FakeElement("url('data:image/svg+xml;base64,AAAA')");
    const local = new FakeElement("url('/Users/me/mask.png')");
    const none = new FakeElement('none');
    await runOn([gradient, inline, local, none]);

    expect(gradient.style.maskImage).toBeUndefined();
    expect(inline.style.maskImage).toBeUndefined();
    expect(local.style.maskImage).toBeUndefined();
    expect(none.style.maskImage).toBeUndefined();
    expect(global.fetch).toBeDefined();
  });

  test('fetches a url shared by several elements only once', async () => {
    const els = [new FakeElement(`url(${REMOTE})`), new FakeElement(`url(${REMOTE})`),
      new FakeElement(`url(${REMOTE})`)];
    const calls = [];
    await runOn(els, async (url) => {
      calls.push(url);
      return { ok: true, blob: async () => ({ __dataUri: DATA_URI }) };
    });

    expect(calls).toEqual([REMOTE]);
    for (const el of els) expect(el.style.maskImage).toBe(`url('${DATA_URI}')`);
  });

  test('keeps the original value and warns when the fetch fails', async () => {
    const el = new FakeElement(`url(${REMOTE})`);
    await runOn([el], async () => ({ ok: false, status: 403 }));

    expect(el.style.maskImage).toBeUndefined();
    expect(warnSpy).toHaveBeenCalledTimes(1);
    expect(warnSpy.mock.calls[0][0]).toContain('failed to inline mask');
  });

  test('survives a rejecting fetch', async () => {
    const el = new FakeElement(`url(${REMOTE})`);
    await runOn([el], async () => {
      throw new Error('network down');
    });

    expect(el.style.maskImage).toBeUndefined();
    expect(warnSpy.mock.calls[0][0]).toContain('network down');
  });
});
