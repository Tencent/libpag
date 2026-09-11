'use strict';

const { isIntrinsicInlineContentWidth } = require('../dist/lib/browser-snapshot');

function computed(values = {}) {
  return {
    display: values.display || 'block',
    visibility: values.visibility || 'visible',
    opacity: values.opacity || '1',
    position: values.position || 'static',
    transform: values.transform || 'none',
    getPropertyValue(prop) {
      return values[prop] == null ? '' : String(values[prop]);
    },
  };
}

describe('isIntrinsicInlineContentWidth', () => {
  const originalNode = global.Node;
  const originalDocument = global.document;
  const originalGetComputedStyle = global.getComputedStyle;

  afterEach(() => {
    global.Node = originalNode;
    global.document = originalDocument;
    global.getComputedStyle = originalGetComputedStyle;
  });

  function fixture({ typedWidth = 'auto', contentWidth = 509.8, hostWidth = 509.8 } = {}) {
    global.Node = { ELEMENT_NODE: 1, TEXT_NODE: 3 };
    const text = { nodeType: 3 };
    const dot = {
      nodeType: 1,
      tagName: 'SPAN',
      childNodes: [text],
      computed: computed({
        display: 'inline',
        position: 'static',
        'margin-left': '14px',
        'margin-right': '14px',
      }),
    };
    const host = {
      childNodes: [text, dot, text],
      computedStyleMap() {
        return { get: () => ({ value: typedWidth }) };
      },
    };
    global.getComputedStyle = (element) => element.computed;
    global.document = {
      createRange() {
        return {
          selectNodeContents() {},
          getBoundingClientRect() {
            return { left: 100, right: 100 + contentWidth, width: contentWidth, height: 41.6 };
          },
          detach() {},
        };
      },
    };
    return {
      host,
      hostComputed: computed({
        'animation-name': 'none',
        'overflow-x': 'visible',
        'line-height': '41.6px',
      }),
      rect: { left: 100, right: 100 + hostWidth, width: hostWidth, height: 41.6 },
    };
  }

  test('recognises an auto-width single-line item with a margin-bearing inline run', () => {
    const f = fixture();
    expect(isIntrinsicInlineContentWidth(
      f.host, f.hostComputed, f.rect, { flexItem: true },
    )).toBe(true);
  });

  test('keeps an authored pixel width fixed even when it happens to equal the content', () => {
    const f = fixture({ typedWidth: 509.8 });
    expect(isIntrinsicInlineContentWidth(
      f.host, f.hostComputed, f.rect, { flexItem: true },
    )).toBe(false);
  });

  test('keeps stretched or reserved auto width when the host is wider than its content', () => {
    const f = fixture({ contentWidth: 509.8, hostWidth: 700 });
    expect(isIntrinsicInlineContentWidth(
      f.host, f.hostComputed, f.rect, { flexItem: true },
    )).toBe(false);
  });
});
