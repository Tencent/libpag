'use strict';

// `dropCollapsedDetailsContent` runs inside `page.evaluate` against a live
// DOM, but its body is plain DOM code, so we drive it in node behind minimal
// `document` / element stubs — the same approach pseudo-materialise.test.js
// takes for `materializeDecorativePseudoElements`.
//
// What it must encode, all verified against Chromium:
//   - a closed `<details>` paints only its *first* `summary` child;
//   - a second `summary` is content and is hidden;
//   - content *before* the summary is hidden too (so the cut is "everything
//     except the first summary", not "everything after it");
//   - a closed `<details>` with no summary paints nothing at all;
//   - an open `<details>` is left completely alone.

const { dropCollapsedDetailsContent } = require('../dist/lib/browser-snapshot');

class FakeText {
  constructor(value) {
    this.nodeType = 3;
    this.nodeValue = value;
    this.parentNode = null;
  }
}

class FakeElement {
  constructor(tag) {
    this.nodeType = 1;
    this.tagName = String(tag).toUpperCase();
    this.childNodes = [];
    this.parentNode = null;
    this.open = false;
  }
  get children() {
    return this.childNodes.filter((n) => n.nodeType === 1);
  }
  appendChild(node) {
    node.parentNode = this;
    this.childNodes.push(node);
    return node;
  }
  removeChild(node) {
    const at = this.childNodes.indexOf(node);
    if (at === -1) throw new Error('removeChild: node is not a child');
    this.childNodes.splice(at, 1);
    node.parentNode = null;
    return node;
  }
}

let savedDocument;

beforeEach(() => {
  savedDocument = global.document;
});

afterEach(() => {
  global.document = savedDocument;
});

// `details` is the list the pass will discover; the stub mirrors the one
// thing it queries.
function runOn(detailsList) {
  global.document = {
    querySelectorAll: (selector) => (selector === 'details' ? detailsList : []),
  };
  return dropCollapsedDetailsContent();
}

function makeDetails(open) {
  const details = new FakeElement('details');
  details.open = Boolean(open);
  return details;
}

function textsOf(node) {
  return node.childNodes.map((c) => (c.nodeType === 1 ? c.tagName : `<text:${c.nodeValue}>`));
}

describe('dropCollapsedDetailsContent — closed <details>', () => {
  test('keeps the first summary and drops every later child', () => {
    const details = makeDetails(false);
    const summary = details.appendChild(new FakeElement('summary'));
    const answer = details.appendChild(new FakeElement('div'));
    const stats = runOn([details]);

    expect(textsOf(details)).toEqual(['SUMMARY']);
    expect(details.childNodes[0]).toBe(summary);
    expect(answer.parentNode).toBe(null);
    expect(stats).toEqual({ details: 1, nodes: 1 });
  });

  test('drops content that precedes the summary as well', () => {
    const details = makeDetails(false);
    const before = details.appendChild(new FakeElement('div'));
    const summary = details.appendChild(new FakeElement('summary'));
    const after = details.appendChild(new FakeElement('div'));
    runOn([details]);

    // Chromium hides a pre-summary sibling too — the details box in the live
    // page measures summary-height only — so leaving it would leak a box.
    expect(textsOf(details)).toEqual(['SUMMARY']);
    expect(details.childNodes[0]).toBe(summary);
    expect(before.parentNode).toBe(null);
    expect(after.parentNode).toBe(null);
  });

  test('treats a second summary as content', () => {
    const details = makeDetails(false);
    const first = details.appendChild(new FakeElement('summary'));
    const second = details.appendChild(new FakeElement('summary'));
    details.appendChild(new FakeElement('div'));
    runOn([details]);

    expect(textsOf(details)).toEqual(['SUMMARY']);
    expect(details.childNodes[0]).toBe(first);
    expect(second.parentNode).toBe(null);
  });

  test('keeps the summary when it is not the first child', () => {
    const details = makeDetails(false);
    details.appendChild(new FakeElement('div'));
    const summary = details.appendChild(new FakeElement('summary'));
    runOn([details]);

    expect(details.childNodes).toEqual([summary]);
  });

  test('drops bare text nodes along with elements', () => {
    const details = makeDetails(false);
    const summary = details.appendChild(new FakeElement('summary'));
    details.appendChild(new FakeText('loose answer text'));
    details.appendChild(new FakeText('\n  '));
    const stats = runOn([details]);

    expect(details.childNodes).toEqual([summary]);
    expect(stats.nodes).toBe(2);
  });

  test('a closed <details> with no summary keeps nothing', () => {
    const details = makeDetails(false);
    details.appendChild(new FakeElement('div'));
    details.appendChild(new FakeElement('p'));
    runOn([details]);

    expect(details.childNodes).toEqual([]);
  });

  test('is idempotent — a second run has nothing left to drop', () => {
    const details = makeDetails(false);
    details.appendChild(new FakeElement('summary'));
    details.appendChild(new FakeElement('div'));

    expect(runOn([details])).toEqual({ details: 1, nodes: 1 });
    expect(runOn([details])).toEqual({ details: 1, nodes: 0 });
    expect(textsOf(details)).toEqual(['SUMMARY']);
  });
});

describe('dropCollapsedDetailsContent — open <details>', () => {
  test('leaves the children untouched', () => {
    const details = makeDetails(true);
    details.appendChild(new FakeElement('summary'));
    details.appendChild(new FakeElement('div'));

    expect(runOn([details])).toEqual({ details: 0, nodes: 0 });
    expect(textsOf(details)).toEqual(['SUMMARY', 'DIV']);
  });

  test('only closed details in a mixed list are pruned', () => {
    const open = makeDetails(true);
    open.appendChild(new FakeElement('summary'));
    open.appendChild(new FakeElement('div'));
    const closed = makeDetails(false);
    closed.appendChild(new FakeElement('summary'));
    closed.appendChild(new FakeElement('div'));
    const stats = runOn([open, closed]);

    expect(textsOf(open)).toEqual(['SUMMARY', 'DIV']);
    expect(textsOf(closed)).toEqual(['SUMMARY']);
    expect(stats).toEqual({ details: 1, nodes: 1 });
  });
});
