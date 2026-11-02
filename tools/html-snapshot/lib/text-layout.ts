// Browser layout text is indexed in UTF-16, independently of DOM node boundaries.
// Keep that information separate from geometry: DOMSnapshot's bounds can use a
// different zoom scale, and its multicolumn boxes do not always include the column
// translation. The in-page emitter measures the corresponding DOM Ranges instead.
import type { EngineName, Page } from './browser-engine';
import { beginUntransformedLayout, endUntransformedLayout } from './control-capture';
import type { ControlText } from './control-capture';

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
  controls?: ControlText[];
  warnings?: string[];
}

// Resolve counters/attr()/quotes using the browser's generated layout text,
// before replacing any pseudo with an addressable DOM stand-in.
export async function capturePseudoContents(page: Page, engine: EngineName):
    Promise<{ path: number[]; pseudo: string; text: string }[]> {
  const session = engine === 'playwright'
    ? await page.context().newCDPSession(page) : await page.createCDPSession();
  try {
    const snapshot = await session.send('DOMSnapshot.captureSnapshot', { computedStyles: [] });
    const { nodes, layout } = snapshot.documents[0];
    const str = (index: number): string => snapshot.strings[index] || '';
    const pseudos = new Map<number, string>((nodes.pseudoType?.index || [])
      .map((index: number, i: number) => [index, str(nodes.pseudoType.value[i])]));
    const shadows = new Set(nodes.shadowRootType?.index || []);
    const paths: (number[] | null)[] = [];
    const counts = new Map<number, number>();
    const owners = new Map<number, number>();
    const entries = new Map<number, { path: number[]; pseudo: string; text: string }>();
    for (let i = 0; i < nodes.nodeType.length; i++) {
      const parent = nodes.parentIndex[i];
      if (i === 0) { paths.push([]); continue; }
      const pseudo = pseudos.get(i);
      if (pseudo || shadows.has(i) || !paths[parent]) {
        paths.push(null);
        if ((pseudo === 'before' || pseudo === 'after') && paths[parent]) {
          owners.set(i, i);
          entries.set(i, { path: paths[parent]!, pseudo: `::${pseudo}`, text: '' });
        } else if (owners.has(parent)) owners.set(i, owners.get(parent)!);
        continue;
      }
      const index = counts.get(parent) || 0;
      counts.set(parent, index + 1);
      paths.push([...paths[parent]!, index]);
    }
    for (let i = 0; i < layout.nodeIndex.length; i++) {
      const owner = owners.get(layout.nodeIndex[i]);
      if (owner !== undefined) entries.get(owner)!.text += str(layout.text[i]);
    }
    return [...entries.values()];
  } finally {
    await session.detach();
  }
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
  const controls = new WeakMap<Node, ControlText>();
  for (const entry of data.controls || []) {
    const node = resolve(entry.path);
    if (node?.nodeType === 1) controls.set(node, entry);
  }
  target.__pagxControlTexts = controls;
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
    // The legacy protocol exposes UA shadow layout. Only layoutText is read:
    // inputValue/nodeValue can contain an unmasked password and must never be
    // copied into metadata, diagnostics, or the exported document.
    const controlPaths = new Map<number, number[]>();
    for (let i = 0; i < paths.length; i++) {
      if (paths[i] && ['INPUT', 'TEXTAREA', 'SELECT'].includes(str(nodes.nodeName[i]))) {
        controlPaths.set(nodes.backendNodeId[i], paths[i]!);
      }
    }
    if (controlPaths.size) {
      await page.evaluate(beginUntransformedLayout);
      try {
        const rects: { width: number; height: number }[] = await page.evaluate((items: number[][]) =>
          items.map(path => {
            let node: Node = document;
            for (const index of path) node = node.childNodes[index];
            const rect = (node as Element).getBoundingClientRect();
            return { width: rect.width, height: rect.height };
          }), [...controlPaths.values()]);
        const legacy = await session.send('DOMSnapshot.getSnapshot', {
          computedStyleWhitelist: TEXT_LAYOUT_PROPERTIES, includeUserAgentShadowTree: true,
        });
        const owner = new Map<number, number>();
        const hosts = new Map<number, { path: number[]; scale: number; x: number; y: number }>();
        const keys = [...controlPaths.keys()];
        for (let i = 0; i < legacy.domNodes.length; i++) {
          const node = legacy.domNodes[i];
          const path = controlPaths.get(node.backendNodeId);
          if (path) {
            const box = legacy.layoutTreeNodes[node.layoutNodeIndex]?.boundingBox;
            const rect = rects[keys.indexOf(node.backendNodeId)];
            if (!box || !rect.width || !box.width) continue;
            hosts.set(i, { path, scale: box.width / rect.width, x: box.x, y: box.y });
            owner.set(i, i);
          }
          const host = owner.get(i);
          if (host !== undefined) {
            for (const child of node.childNodeIndexes || []) owner.set(child, host);
          }
        }
        const controls = new Map<number, ControlText>();
        for (const [index, host] of hosts) controls.set(index, { path: host.path, fragments: [] });
        for (const layoutNode of legacy.layoutTreeNodes) {
          const hostIndex = owner.get(layoutNode.domNodeIndex);
          const host = hostIndex === undefined ? undefined : hosts.get(hostIndex);
          if (!host || !layoutNode.layoutText) continue;
          const style = Object.fromEntries(
            (legacy.computedStyles[layoutNode.styleIndex]?.properties || [])
              .map((p: { name: string; value: string }) => [p.name, p.value]),
          );
          for (const inline of layoutNode.inlineTextNodes || []) {
            const text = layoutNode.layoutText.slice(
              inline.startCharacterIndex, inline.startCharacterIndex + inline.numCharacters,
            );
            const box = inline.boundingBox;
            if (!text || !box.width || !box.height) continue;
            controls.get(hostIndex!)!.fragments.push({
              text, style, x: (box.x - host.x) / host.scale, y: (box.y - host.y) / host.scale,
              width: box.width / host.scale, height: box.height / host.scale,
            });
          }
        }
        result.controls = [...controls.values()];
      } catch {
        result.warnings!.push('html-snapshot: native text layout unavailable; using control text fallback');
      } finally {
        await page.evaluate(endUntransformedLayout);
      }
    }
    return result;
  } finally {
    await session.detach();
  }
}
