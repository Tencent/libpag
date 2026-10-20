// Browser layout text is indexed in UTF-16, independently of DOM node boundaries.
// Keep that information separate from geometry: DOMSnapshot's bounds can use a
// different zoom scale, and its multicolumn boxes do not always include the column
// translation. The in-page emitter measures the corresponding DOM Ranges instead.
import type { EngineName, Page } from './browser-engine';

export const TEXT_LAYOUT_PROPERTIES = [
  'font-family', 'font-size', 'font-weight', 'font-style', 'font-stretch',
  'font-variation-settings', 'font-feature-settings', 'font-kerning',
  'color', '-webkit-text-fill-color', '-webkit-text-stroke-width',
  '-webkit-text-stroke-color', 'line-height', 'letter-spacing', 'word-spacing',
  'text-transform', 'text-decoration', 'text-decoration-color', 'text-shadow',
  'white-space', 'writing-mode', 'direction', 'float', 'background-color',
  'padding-top', 'padding-right', 'padding-bottom', 'padding-left',
  'border-top-width', 'border-right-width', 'border-bottom-width', 'border-left-width',
];

export interface LayoutTextRun {
  text: string;
  style: Record<string, string>;
  boxes: { start: number; length: number }[];
}
export interface TextLayoutData {
  texts: { path: number[]; source: string; runs: LayoutTextRun[] }[];
  initials: { path: number[]; runs: LayoutTextRun[] }[];
  warnings?: string[];
}

// Deliberately self-contained: also exposed by the standalone browser bundle.
// Paths count real childNodes, not the pseudo nodes in the protocol tree.
export function installTextLayout(data: TextLayoutData): void {
  const resolve = (path: number[]): Node | null => {
    let node: Node = document;
    for (const index of path) {
      if (!node.childNodes[index]) return null;
      node = node.childNodes[index];
    }
    return node;
  };
  const texts = new WeakMap<Node, { source: string; runs: LayoutTextRun[] }>();
  const initials = new WeakMap<Node, LayoutTextRun[]>();
  const initialHosts: { node: Node; runs: LayoutTextRun[] }[] = [];
  for (const entry of data.texts) {
    const node = resolve(entry.path);
    if (node?.nodeType === 3 && node.nodeValue === entry.source) texts.set(node, entry);
  }
  for (const entry of data.initials) {
    const node = resolve(entry.path);
    if (node?.nodeType === 1) {
      initials.set(node, entry.runs);
      initialHosts.push({ node, runs: entry.runs });
    }
  }
  const target = window as unknown as Record<string, unknown>;
  target.__pagxTextLayouts = texts;
  target.__pagxInitialLayouts = initials;
  target.__pagxInitialHosts = initialHosts;
  target.__pagxTextLayoutWarnings = data.warnings || [];
}

// Protocol adapters live outside browser-snapshot so the browser bundle has no
// dependency on Puppeteer, Playwright, or privileged debugging APIs.
export async function captureTextLayout(page: Page, engine: EngineName): Promise<TextLayoutData> {
  const session = engine === 'playwright'
    ? await page.context().newCDPSession(page)
    : await page.createCDPSession();
  try {
    const snapshot = await session.send('DOMSnapshot.captureSnapshot', {
      computedStyles: TEXT_LAYOUT_PROPERTIES,
    });
    const doc = snapshot.documents[0];
    const { nodes, layout, textBoxes } = doc;
    const str = (index: number): string => snapshot.strings[index] || '';
    const pseudos = new Map<number, string>(
      (nodes.pseudoType?.index || []).map((index: number, i: number) =>
        [index, str(nodes.pseudoType.value[i])]),
    );
    const shadowRoots = new Set<number>(nodes.shadowRootType?.index || []);
    const paths: (number[] | null)[] = [];
    const childCounts = new Map<number, number>();
    for (let i = 0; i < nodes.nodeType.length; i++) {
      const parent = nodes.parentIndex[i];
      if (i === 0) { paths.push([]); continue; }
      if (pseudos.has(i) || shadowRoots.has(i) || !paths[parent]) {
        paths.push(null);
        continue;
      }
      const childIndex = childCounts.get(parent) || 0;
      childCounts.set(parent, childIndex + 1);
      paths.push([...paths[parent]!, childIndex]);
    }
    const boxes = new Map<number, { start: number; length: number }[]>();
    for (let i = 0; i < textBoxes.layoutIndex.length; i++) {
      const index = textBoxes.layoutIndex[i];
      if (!boxes.has(index)) boxes.set(index, []);
      boxes.get(index)!.push({ start: textBoxes.start[i], length: textBoxes.length[i] });
    }
    const runs = new Map<number, LayoutTextRun[]>();
    for (let i = 0; i < layout.nodeIndex.length; i++) {
      const text = str(layout.text[i]);
      if (!text || !boxes.has(i)) continue;
      const index = layout.nodeIndex[i];
      if (!runs.has(index)) runs.set(index, []);
      runs.get(index)!.push({
        text,
        style: Object.fromEntries(TEXT_LAYOUT_PROPERTIES.map((p, j) =>
          [p, str(layout.styles[i][j])])),
        boxes: boxes.get(i)!,
      });
    }
    const result: TextLayoutData = { texts: [], initials: [], warnings: [] };
    for (let i = 0; i < nodes.nodeType.length; i++) {
      const path = paths[i];
      if (path && nodes.nodeType[i] === 3) {
        const source = str(nodes.nodeValue[i]);
        // Missing layout runs matter too: a text node can consist entirely of
        // the first-letter, whose layout object belongs to the pseudo instead.
        if (source.trim()) result.texts.push({ path, source, runs: runs.get(i) || [] });
      } else if (pseudos.get(i) === 'first-letter' && runs.has(i)) {
        const parent = nodes.parentIndex[i];
        if (paths[parent]) result.initials.push({ path: paths[parent]!, runs: runs.get(i)! });
        else result.warnings!.push('html-snapshot: text: generated/shadow first-letter has no ' +
          'addressable DOM Range; its pseudo style cannot be preserved');
      }
    }
    return result;
  } finally {
    await session.detach();
  }
}
