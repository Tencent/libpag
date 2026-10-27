/** @jest-environment jsdom */
'use strict';

const {
  TEXT_LAYOUT_PROPERTIES, capturePseudoContents, captureTextLayout, installTextLayout,
} = require('../dist/lib/text-layout');
const { computedStyle, setRect, nodePath } = require('./dom-helpers');

// Build the columnar protocol format from readable nodes. Pseudo/shadow nodes
// occupy protocol indexes but must not advance an addressable DOM child path.
function snapshotFixture(entries, layouts = []) {
  const strings = [''];
  const intern = value => {
    if (!strings.includes(value)) strings.push(value);
    return strings.indexOf(value);
  };
  const nodes = { nodeType: [], parentIndex: [], nodeValue: [], nodeName: [], backendNodeId: [] };
  entries.forEach((entry, i) => {
    nodes.nodeType.push(entry.type ?? 1);
    nodes.parentIndex.push(entry.parent ?? -1);
    nodes.nodeValue.push(intern(entry.value || ''));
    nodes.nodeName.push(intern(entry.name || 'DIV'));
    nodes.backendNodeId.push(entry.backend ?? i + 100);
    for (const [key, value] of [['pseudoType', entry.pseudo], ['shadowRootType', entry.shadow]]) {
      if (!value) continue;
      nodes[key] ||= { index: [], value: [] };
      nodes[key].index.push(i);
      nodes[key].value.push(intern(value));
    }
  });
  const layout = { nodeIndex: [], text: [], styles: [] };
  const textBoxes = { layoutIndex: [], start: [], length: [] };
  layouts.forEach((entry, i) => {
    layout.nodeIndex.push(entry.node);
    layout.text.push(intern(entry.text || ''));
    layout.styles.push(TEXT_LAYOUT_PROPERTIES.map(name => intern(entry.style?.[name] || '')));
    for (const box of entry.boxes || []) {
      textBoxes.layoutIndex.push(i);
      textBoxes.start.push(box.start);
      textBoxes.length.push(box.length);
    }
  });
  return { strings, documents: [{ nodes, layout, textBoxes }] };
}

function protocolPage(snapshot, legacy) {
  const session = {
    send: jest.fn(async method => {
      if (method === 'DOMSnapshot.captureSnapshot') return snapshot;
      if (legacy instanceof Error) throw legacy;
      return legacy;
    }),
    detach: jest.fn().mockResolvedValue(undefined),
  };
  const context = { newCDPSession: jest.fn().mockResolvedValue(session) };
  const page = {
    createCDPSession: jest.fn().mockResolvedValue(session),
    context: () => context,
    evaluate: jest.fn(async (fn, arg) => fn(arg)),
  };
  return { page, session, context };
}

beforeEach(() => {
  document.body.innerHTML = '';
  jest.spyOn(window, 'getComputedStyle').mockImplementation(el => computedStyle(el));
});
afterEach(() => { jest.restoreAllMocks(); });

describe('capturePseudoContents', () => {
  test.each(['puppeteer', 'playwright'])('%s resolves nested generated text without shifting sibling paths', async engine => {
    const snapshot = snapshotFixture([
      { type: 9 }, { parent: 0 },
      { parent: 1, pseudo: 'before' }, { parent: 2, type: 3 },
      { parent: 3, type: 3 }, { parent: 1, type: 3, value: 'body' },
      { parent: 1, pseudo: 'marker' }, { parent: 6, type: 3 },
      { parent: 1, shadow: 'open' }, { parent: 8 },
      { parent: 1 }, { parent: 10, pseudo: 'after' }, { parent: 11, type: 3 },
      { parent: 1, pseudo: 'after' }, { parent: 13, type: 3 },
    ], [
      { node: 3, text: 'Chapter ' }, { node: 4, text: '2' },
      { node: 5, text: 'body' }, { node: 7, text: 'ignored marker' },
      { node: 9, text: 'shadow' }, { node: 12, text: 'end' },
      { node: 14, text: '' },
    ]);
    const { page, session, context } = protocolPage(snapshot);
    expect(await capturePseudoContents(page, engine)).toEqual([
      { path: [0], pseudo: '::before', text: 'Chapter 2' },
      { path: [0, 1], pseudo: '::after', text: 'end' },
      { path: [0], pseudo: '::after', text: '' },
    ]);
    expect(session.send).toHaveBeenCalledWith('DOMSnapshot.captureSnapshot', { computedStyles: [] });
    expect(session.detach).toHaveBeenCalledTimes(1);
    if (engine === 'playwright') expect(context.newCDPSession).toHaveBeenCalledWith(page);
    else expect(page.createCDPSession).toHaveBeenCalledTimes(1);
  });

  test('works when the protocol omits pseudo and shadow arrays', async () => {
    const { page } = protocolPage(snapshotFixture([{ type: 9 }, { parent: 0 }]));
    expect(await capturePseudoContents(page, 'puppeteer')).toEqual([]);
  });

  test.each([capturePseudoContents, captureTextLayout])('%p detaches the session after protocol failure', async capture => {
    const { page, session } = protocolPage(null);
    const error = new Error('protocol disconnected');
    session.send.mockRejectedValue(error);
    await expect(capture(page, 'puppeteer')).rejects.toBe(error);
    expect(session.detach).toHaveBeenCalledTimes(1);
  });
});

describe('installTextLayout', () => {
  test('installs only matching text sources and valid element paths, and replaces old metadata', () => {
    document.body.innerHTML = '<p>Hello<span>changed</span></p><input>';
    const host = document.querySelector('p');
    const text = host.firstChild;
    const changed = host.lastChild.firstChild;
    const input = document.querySelector('input');
    const runs = [{ text: 'Hello', style: { color: 'red' }, boxes: [{ start: 0, length: 5 }] }];
    const entry = { path: nodePath(text), source: 'Hello', runs };
    const control = { path: nodePath(input), fragments: [] };
    installTextLayout({
      texts: [entry, { path: nodePath(changed), source: 'old', runs },
        { path: nodePath(host), source: null, runs }, { path: [99, 0], source: 'Hello', runs }],
      initials: [{ path: nodePath(host), runs }, { path: nodePath(text), runs }, { path: [99], runs }],
      controls: [control, { path: nodePath(text) }, { path: [99] }],
      warnings: ['fallback'],
    });
    expect(window.__pagxTextLayouts.get(text)).toBe(entry);
    expect(window.__pagxTextLayouts.has(changed)).toBe(false);
    expect(window.__pagxTextLayouts.has(host)).toBe(false);
    expect(window.__pagxInitialLayouts.get(host)).toBe(runs);
    expect(window.__pagxInitialHosts).toEqual([{ node: host, runs }]);
    expect(window.__pagxControlTexts.get(input)).toBe(control);
    expect(window.__pagxControlTexts.has(text)).toBe(false);
    expect(window.__pagxTextLayoutWarnings).toEqual(['fallback']);
    installTextLayout({ texts: [], initials: [] });
    expect(window.__pagxTextLayouts.has(text)).toBe(false);
    expect(window.__pagxInitialHosts).toEqual([]);
    expect(window.__pagxControlTexts.has(input)).toBe(false);
    expect(window.__pagxTextLayoutWarnings).toEqual([]);
  });
});

describe('captureTextLayout', () => {
  test.each(['puppeteer', 'playwright'])('%s keeps UTF-16 ranges, separate runs and first-letter ownership', async engine => {
    const snapshot = snapshotFixture([
      { type: 9 }, { parent: 0 },
      { parent: 1, type: 3, value: 'A😀B' },
      { parent: 1, pseudo: 'first-letter' },
      { parent: 1, type: 3, value: 'X' },
      { parent: 1, type: 3, value: ' \n' },
      { parent: 1, pseudo: 'before' },
      { parent: 6, pseudo: 'first-letter' },
      { parent: 1, shadow: 'open' },
      { parent: 8, type: 3, value: 'hidden' },
      { parent: 8, pseudo: 'first-letter' },
    ], [
      { node: 2, text: '😀B', style: { color: 'red' }, boxes: [{ start: 0, length: 2 }, { start: 2, length: 1 }] },
      { node: 2, text: 'B', boxes: [{ start: 0, length: 1 }] },
      { node: 3, text: 'A', boxes: [{ start: 0, length: 1 }] },
      { node: 4, text: 'X' }, // Entire source belongs to a first-letter pseudo.
      { node: 5, text: '' },
      { node: 7, text: '“', boxes: [{ start: 0, length: 1 }] },
      { node: 10, text: 'h', boxes: [{ start: 0, length: 1 }] },
    ]);
    const { page, session } = protocolPage(snapshot);
    const result = await captureTextLayout(page, engine);
    expect(result.texts).toHaveLength(2);
    expect(result.texts[0]).toMatchObject({ path: [0, 0], source: 'A😀B', runs: [
      { text: '😀B', style: { color: 'red', 'font-size': '' }, boxes: [{ start: 0, length: 2 }, { start: 2, length: 1 }] },
      { text: 'B', boxes: [{ start: 0, length: 1 }] },
    ] });
    expect(result.texts[1]).toEqual({ path: [0, 1], source: 'X', runs: [] });
    expect(result.initials).toMatchObject([{ path: [0], runs: [{ text: 'A' }] }]);
    expect(result.warnings).toHaveLength(2);
    expect(result.warnings.every(w => /generated\/shadow first-letter/.test(w))).toBe(true);
    expect(page.evaluate).not.toHaveBeenCalled();
    expect(session.send).toHaveBeenCalledWith('DOMSnapshot.captureSnapshot', { computedStyles: TEXT_LAYOUT_PROPERTIES });
    expect(session.detach).toHaveBeenCalledTimes(1);
  });

  function controlFixture() {
    document.body.innerHTML = '<input type="password" value="secret" style="transform:scale(2)"><select></select><textarea></textarea>';
    document.querySelectorAll('input,select,textarea').forEach(el => setRect(el, { width: 100, height: 20 }));
    const snapshot = snapshotFixture([
      { type: 9 }, { type: 10, parent: 0, name: 'html' },
      { parent: 0, name: 'HTML' }, { parent: 2, name: 'HEAD' },
      { parent: 2, name: 'BODY' },
      { parent: 4, name: 'INPUT', backend: 42 },
      { parent: 4, name: 'SELECT', backend: 43 },
      { parent: 4, name: 'TEXTAREA', backend: 44 },
    ]);
    const legacy = {
      domNodes: [
        { backendNodeId: 42, layoutNodeIndex: 0, childNodeIndexes: [1], inputValue: 'secret' },
        { childNodeIndexes: [2] }, { nodeValue: 'secret' },
        { backendNodeId: 43, layoutNodeIndex: 4 },
        { backendNodeId: 44, layoutNodeIndex: 99 },
        { backendNodeId: 999 },
      ],
      layoutTreeNodes: [
        { domNodeIndex: 0, boundingBox: { x: 20, y: 40, width: 200, height: 40 } },
        { domNodeIndex: 2, layoutText: '••😀', styleIndex: 0, inlineTextNodes: [
          { startCharacterIndex: 0, numCharacters: 2, boundingBox: { x: 24, y: 44, width: 16, height: 24 } },
          { startCharacterIndex: 2, numCharacters: 2, boundingBox: { x: 40, y: 44, width: 20, height: 24 } },
          { startCharacterIndex: 4, numCharacters: 1, boundingBox: { width: 1, height: 1 } },
          { startCharacterIndex: 0, numCharacters: 1, boundingBox: { width: 0, height: 1 } },
          { startCharacterIndex: 0, numCharacters: 1, boundingBox: { width: 1, height: 0 } },
        ] },
        { domNodeIndex: 2, layoutText: 'ignored without boxes', styleIndex: -1 },
        { domNodeIndex: 5, layoutText: 'unowned', inlineTextNodes: [] },
        { domNodeIndex: 3, boundingBox: { width: 0 } },
      ],
      computedStyles: [{ properties: [{ name: 'color', value: 'red' }] }],
    };
    return { snapshot, legacy };
  }

  test('uses only rendered control text, scales bounds and restores transformed layout', async () => {
    const { snapshot, legacy } = controlFixture();
    const { page, session } = protocolPage(snapshot, legacy);
    const original = document.body.innerHTML;
    const result = await captureTextLayout(page, 'puppeteer');
    expect(result.controls).toEqual([{ path: nodePath(document.querySelector('input')), fragments: [
      { text: '••', style: { color: 'red' }, x: 2, y: 2, width: 8, height: 12 },
      { text: '😀', style: { color: 'red' }, x: 10, y: 2, width: 10, height: 12 },
    ] }]);
    expect(JSON.stringify(result)).not.toContain('secret');
    expect(result.warnings).toEqual([]);
    expect(document.body.innerHTML).toBe(original);
    expect(window.__pagxCaptureTransforms).toBeUndefined();
    expect(session.send).toHaveBeenCalledWith('DOMSnapshot.getSnapshot', {
      computedStyleWhitelist: TEXT_LAYOUT_PROPERTIES, includeUserAgentShadowTree: true,
    });
    expect(session.detach).toHaveBeenCalledTimes(1);
  });

  test('reports a native-text fallback and restores styles when the legacy protocol fails', async () => {
    const { snapshot } = controlFixture();
    const { page, session } = protocolPage(snapshot, new Error('unsupported method'));
    const original = document.body.innerHTML;
    const result = await captureTextLayout(page, 'playwright');
    expect(result.controls).toBeUndefined();
    expect(result.warnings).toEqual(['html-snapshot: native text layout unavailable; using control text fallback']);
    expect(document.body.innerHTML).toBe(original);
    expect(session.detach).toHaveBeenCalledTimes(1);
  });
});
