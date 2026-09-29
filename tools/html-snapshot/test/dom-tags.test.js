'use strict';

const { DROP_TAG_NAMES } = require('../dist/lib/dom-tags');

describe('DROP_TAG_NAMES', () => {
  test('is a non-empty array', () => {
    expect(Array.isArray(DROP_TAG_NAMES)).toBe(true);
    expect(DROP_TAG_NAMES.length).toBeGreaterThan(0);
  });

  test('includes the structural / non-visual tags the pipeline drops', () => {
    for (const tag of ['script', 'style', 'head', 'meta', 'br', 'form', 'iframe']) {
      expect(DROP_TAG_NAMES).toContain(tag);
    }
  });

  test('excludes tags whose box paints', () => {
    // `<details>` / `<summary>` used to sit in this list, which erased every
    // accordion from the snapshot even though the browser renders the summary
    // row. Their collapsed content is pruned by `dropCollapsedDetailsContent`
    // instead — see test/collapsed-details.test.js.
    for (const tag of ['details', 'summary']) {
      expect(DROP_TAG_NAMES).not.toContain(tag);
    }
  });

  test('entries are all lower-case (canonical form)', () => {
    for (const tag of DROP_TAG_NAMES) {
      expect(typeof tag).toBe('string');
      expect(tag).toBe(tag.toLowerCase());
    }
  });

  test('contains no duplicates', () => {
    expect(new Set(DROP_TAG_NAMES).size).toBe(DROP_TAG_NAMES.length);
  });
});
