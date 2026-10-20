// Browser-only state is passed through explicit providers. The snapshot bundle
// can install the same data without depending on a debugging protocol.
import type { EngineName, Page } from './browser-engine';

export interface ControlTextFragment {
  text: string;
  style: Record<string, string>;
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface ControlText {
  path: number[];
  fragments: ControlTextFragment[];
}

export interface ControlPaint {
  path: number[];
  src: string;
  x: number;
  y: number;
  width: number;
  height: number;
}

export function installControlPaints(paints: ControlPaint[]): void {
  const entries = new WeakMap<Node, ControlPaint>();
  for (const entry of paints) {
    let node: Node | undefined = document;
    for (const index of entry.path) node = node?.childNodes[index];
    if (node?.nodeType === 1) entries.set(node, entry);
  }
  (window as unknown as Record<string, unknown>).__pagxControlPaints = entries;
}

// Keep transform-created containing blocks while measuring untransformed boxes.
// Changing transform to `none` would move absolute/fixed descendants.
export function beginUntransformedLayout(): void {
  const changes: { element: HTMLElement; style: string | null }[] = [];
  for (const element of document.querySelectorAll<HTMLElement>('*')) {
    if (!element.style) continue;
    const cs = getComputedStyle(element);
    const overrides: [string, string][] = [];
    if (cs.transform !== 'none') overrides.push(['transform', 'matrix(1,0,0,1,0,0)']);
    if (cs.rotate !== 'none') overrides.push(['rotate', '0deg']);
    if (cs.scale !== 'none') overrides.push(['scale', '1']);
    if (cs.translate !== 'none') overrides.push(['translate', '0px']);
    if (!overrides.length) continue;
    changes.push({ element, style: element.getAttribute('style') });
    for (const [name, value] of overrides) element.style.setProperty(name, value, 'important');
  }
  (window as unknown as Record<string, unknown>).__pagxCaptureTransforms = changes;
}

export function endUntransformedLayout(): void {
  const state = window as unknown as Record<string, unknown>;
  const changes = state.__pagxCaptureTransforms as
    { element: HTMLElement; style: string | null }[] | undefined;
  for (const { element, style } of changes || []) {
    if (style === null) {
      element.style.cssText = '';
      element.getAttribute('style');
      element.removeAttribute('style');
    }
    else element.setAttribute('style', style);
  }
  delete state.__pagxCaptureTransforms;
}

// Isolate the native paint on a transparent canvas. Never crop the composited
// page: that would bake ancestor backgrounds, opacity and overlapping siblings
// into the control, then apply them a second time in the exported document.
export async function captureControlPaints(page: Page, engine: EngineName): Promise<ControlPaint[]> {
  await page.evaluate(beginUntransformedLayout);
  const paints: ControlPaint[] = [];
  try {
    const paths: number[][] = await page.evaluate(() => {
      const result: number[][] = [];
      for (const element of document.querySelectorAll('input,select,progress,meter,fieldset')) {
        const cs = getComputedStyle(element);
        if (cs.display === 'none' || cs.visibility !== 'visible') continue;
        const tag = element.localName;
        const type = (element as HTMLInputElement).type;
        if (tag === 'input' && ![
          'checkbox', 'radio', 'range', 'color', 'date', 'datetime-local', 'month',
          'time', 'week', 'file', 'image', 'number', 'search',
        ].includes(type)) continue;
        if ((type === 'checkbox' || type === 'radio' || tag === 'select') &&
            cs.appearance === 'none') continue;
        const rect = element.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0) continue;
        const path: number[] = [];
        let node: Node = element;
        while (node.parentNode) {
          path.unshift(Array.prototype.indexOf.call(node.parentNode.childNodes, node));
          node = node.parentNode;
        }
        result.push(path);
      }
      if (result.length) {
        const originals = (window as unknown as Record<string, unknown>).__pagxCaptureTransforms as
          { element: HTMLElement; style: string | null }[];
        const recorded = new Set(originals.map(entry => entry.element));
        for (const element of document.querySelectorAll<HTMLElement>('*')) {
          if (!element.style) continue;
          if (!recorded.has(element)) originals.push({ element, style: element.getAttribute('style') });
          // Inline important wins even over authored visibility:visible!important.
          // Visibility changes paint only, so all source layout slots remain.
          element.style.setProperty('visibility', 'hidden', 'important');
        }
      }
      return result;
    });
    for (const path of paths) {
      try {
        const bounds = await page.evaluate((nodePath: number[]) => {
          let node: Node = document;
          for (const index of nodePath) node = node.childNodes[index];
          const element = node as HTMLElement;
          const computed = getComputedStyle(element);
          const size = { width: computed.width, height: computed.height };
          const outline = computed.outlineStyle === 'none' ? 0 :
            Math.max(0, parseFloat(computed.outlineWidth) + parseFloat(computed.outlineOffset));
          const saved: { element: HTMLElement; style: string | null }[] = [];
          // Snapshot the complete declarations so priorities and the absence of
          // an inline style are restored exactly, including on failure.
          const save = (el: HTMLElement) => saved.push({ element: el, style: el.getAttribute('style') });
          save(element);
          for (let parent = element.parentElement; parent; parent = parent.parentElement) save(parent);
          const sheet = document.createElement('style');
          (window as unknown as Record<string, unknown>).__pagxIsolatedControl = { saved, sheet };
          // Important rules in the first cascade layer outrank author pseudo
          // rules, including a pseudo with an explicit visible!important.
          sheet.textContent = '@layer pagx-capture-isolation {' +
            '*::before,*::after { visibility: hidden !important; }}';
          document.head.insertBefore(sheet, document.head.firstChild);
          for (const { element: el } of saved) {
            for (const [key, value] of Object.entries({
              opacity: '1', filter: 'none', 'backdrop-filter': 'none',
              'mix-blend-mode': 'normal', 'clip-path': 'none', mask: 'none',
              overflow: 'visible', transition: 'none',
            })) el.style.setProperty(key, value, 'important');
          }
          // Root canvas backgrounds are propagated independently of visibility.
          for (const el of [document.documentElement, document.body]) {
            if (el) el.style.setProperty('background', 'transparent', 'important');
          }
          element.style.setProperty('visibility', 'visible', 'important');
          element.style.setProperty('width', size.width, 'important');
          element.style.setProperty('height', size.height, 'important');
          // CSS effects belong to the emitted wrapper. The image contains only
          // the control's own surface, including native internal decorations.
          element.style.setProperty('box-shadow', 'none', 'important');
          element.style.setProperty('caret-color', 'transparent', 'important');
          let rect = element.getBoundingClientRect();
          // Move off-canvas boxes into the capture surface without changing
          // layout or losing the clipped portion of their native appearance.
          const moveX = Math.max(0, Math.ceil(outline - rect.left - scrollX));
          const moveY = Math.max(0, Math.ceil(outline - rect.top - scrollY));
          if (moveX || moveY) {
            element.style.setProperty('transform', `translate(${moveX}px,${moveY}px)`, 'important');
            rect = element.getBoundingClientRect();
          }
          const dpr = window.devicePixelRatio || 1;
          const left = Math.floor((rect.left + scrollX - outline) * dpr) / dpr;
          const top = Math.floor((rect.top + scrollY - outline) * dpr) / dpr;
          const right = Math.ceil((rect.right + scrollX + outline) * dpr) / dpr;
          const bottom = Math.ceil((rect.bottom + scrollY + outline) * dpr) / dpr;
          return {
            left, top, width: right - left, height: bottom - top,
            x: left - rect.left - scrollX, y: top - rect.top - scrollY,
          };
        }, path);
        // Off-canvas paint is still clipped by the document. Avoid passing an
        // invalid protocol clip and report it through the missing-provider path.
        if (bounds.left < 0 || bounds.top < 0) continue;
        const bytes = await page.screenshot({
          type: 'png', omitBackground: true,
          clip: { x: bounds.left, y: bounds.top, width: bounds.width, height: bounds.height },
          ...(engine === 'puppeteer' ? { captureBeyondViewport: true } : {}),
        });
        paints.push({
          path, src: `data:image/png;base64,${Buffer.from(bytes).toString('base64')}`,
          x: bounds.x, y: bounds.y, width: bounds.width, height: bounds.height,
        });
      } finally {
        await page.evaluate(() => {
          const state = window as unknown as Record<string, unknown>;
          const isolated = state.__pagxIsolatedControl as {
            saved: { element: HTMLElement; style: string | null }[];
            sheet: HTMLStyleElement;
          } | undefined;
          if (!isolated) return;
          isolated.sheet.remove();
          for (const { element, style } of isolated.saved) {
            if (style === null) {
              element.style.cssText = '';
              element.getAttribute('style');
              element.removeAttribute('style');
            }
            else element.setAttribute('style', style);
          }
          delete state.__pagxIsolatedControl;
        });
      }
    }
    return paints;
  } finally {
    await page.evaluate(endUntransformedLayout);
  }
}
