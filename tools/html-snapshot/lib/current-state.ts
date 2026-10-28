// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0 (the "License").
// You may obtain a copy at http://www.apache.org/licenses/LICENSE-2.0

import { launchBrowser, newPage, type EngineName, type EngineWrapper, type Page } from './browser-engine';
import {
  SNAPSHOT_INIT_SCRIPT, inlineExternalImages, inlineCanvases,
  materializeDecorativePseudoElements,
} from './browser-snapshot';
import { inlineIconFontsOnPage, ICON_FONT_INIT_SCRIPT } from './icon-font';
import type { StateResource } from './state-resources';
import type { SavedFont } from './font-download';

export interface CaptureOptions {
  extent?: 'viewport' | 'document';
  unsupported?: 'error' | 'rasterize';
  timeoutMs?: number;
  inlineIconFonts?: boolean;
  resources?: Map<string, StateResource>;
}

export interface CapturedBox {
  id: number;
  tag: string;
  rect: number[];
  scroll: number[];
}

/** An inert, computed-style copy of ONE rendering state, not reloadable source HTML. */
export interface CapturedPage {
  version: 1;
  html: string;
  url: string;
  viewport: { width: number; height: number };
  scroll: number[];
  boxes: CapturedBox[];
  unsupported: string[];
  webFonts?: string[];
}

export interface CurrentStateResult {
  html: string;
  width: number;
  height: number;
  warnings: string[];
  url: string;
  fonts?: SavedFont[];
}

export interface CaptureResources {
  urls?: Record<string, string>;
  stylesheets?: Record<string, string>;
}

export class StateCaptureError extends Error {}

/**
 * Runs synchronously in the source document. All writes are to detached clones.
 * Do not add await, scrolling, focus changes, animation cancellation, or styles
 * to the live document here. Also exported by the standalone browser bundle.
 */
export function collectCurrentState(resources: CaptureResources = {}): CapturedPage {
  // Importing into an inert document avoids running custom-element constructors
  // or connected callbacks while making the detached copy.
  const inert = document.implementation.createHTMLDocument('');
  const boxes: CapturedBox[] = [];
  const unsupported: string[] = [];
  const pseudos: string[] = [];
  const escape = (value: string) => value.replace(/&/g, '&amp;').replace(/"/g, '&quot;')
    .replace(/</g, '&lt;').replace(/>/g, '&gt;');
  const rewriteUrls = (value: string, base = document.baseURI): string => value.replace(
    /url\(\s*(?:"((?:\\.|[^"\\])*)"|'((?:\\.|[^'\\])*)'|([^)]*))\s*\)/gi,
    (original, double, single, unquoted) => {
      const raw = String(double ?? single ?? unquoted).trim().replace(/\\(["'\\()])/g, '$1');
      if (raw.startsWith('#') || raw.startsWith('data:')) return original;
      try {
        const url = new URL(raw, base);
        const fragment = url.hash;
        url.hash = '';
        const result = resources.urls?.[url.href];
        return `url("${(result ? result + fragment : new URL(raw, base).href).replace(/"/g, '%22')}")`;
      } catch (_) { return original; }
    });
  const styles = (computed: CSSStyleDeclaration): string => {
    const result: string[] = [];
    for (const key of Array.from(computed)) {
      // Copy actual values now: CSSStyleDeclaration is a live view.
      if (key.startsWith('animation') || key.startsWith('transition')) continue;
      result.push(`${key}:${rewriteUrls(computed.getPropertyValue(key))} !important`);
    }
    result.push('animation:none !important', 'transition:none !important', 'scroll-behavior:auto !important');
    return result.join(';');
  };
  const copy = (source: Node): Node | null => {
    if (source.nodeType === Node.TEXT_NODE) return inert.createTextNode(source.nodeValue || '');
    if (!(source instanceof Element)) return null;
    const tag = source.localName;
    if (tag === 'head') return inert.createElement('head');
    if (['script', 'style', 'link', 'meta', 'base', 'noscript'].includes(tag)) return null;
    const computed = getComputedStyle(source);
    const rect = source.getBoundingClientRect();
    const painted = computed.display !== 'none' && computed.visibility !== 'hidden' &&
      Number(computed.opacity) !== 0 && rect.width > 0 && rect.height > 0;
    const label = `${tag}${source.id ? `#${source.id}` : ''}`;
    if (painted && (source.shadowRoot || tag.includes('-') ||
        ['iframe', 'object', 'embed', 'video', 'audio', 'details', 'dialog', 'meter', 'progress'].includes(tag))) {
      unsupported.push(`${label}: embedded content requires rasterization`);
    }
    if (painted && source.matches(':modal, :popover-open')) {
      unsupported.push(`${label}: browser top-layer content requires rasterization`);
    }
    if (painted && source instanceof HTMLInputElement &&
        !['text', 'search', 'email', 'url', 'tel', 'number', 'hidden'].includes(source.type)) {
      unsupported.push(`${label}: ${source.type} control requires rasterization`);
    }
    let clone = inert.importNode(source, false) as Element;
    if (source instanceof HTMLCanvasElement) {
      try {
        const image = inert.createElement('img');
        image.src = source.toDataURL('image/png');
        clone = image;
      } catch (_) {
        if (painted) unsupported.push(`${label}: canvas pixels are not readable`);
      }
    }
    for (const attr of Array.from(clone.attributes)) {
      if (/^on/i.test(attr.name) || ['srcdoc', 'autofocus', 'srcset', 'sizes'].includes(attr.name)) {
        clone.removeAttribute(attr.name);
      }
    }
    const id = boxes.length;
    clone.setAttribute('data-pagx-capture-id', String(id));
    clone.setAttribute('style', styles(computed));
    boxes.push({ id, tag: label, rect: [rect.x, rect.y, rect.width, rect.height],
      scroll: [source.scrollLeft, source.scrollTop] });
    if (source instanceof HTMLImageElement) {
      let src = source.currentSrc || source.src;
      // Freeze the decoded frame, including blob URLs and animated images, in
      // the source context. This only writes a detached canvas.
      if (source.complete && source.naturalWidth > 0) {
        try {
          const canvas = inert.createElement('canvas');
          canvas.width = source.naturalWidth; canvas.height = source.naturalHeight;
          canvas.getContext('2d')!.drawImage(source, 0, 0);
          src = canvas.toDataURL('image/png');
        } catch (_) {
          const recorded = resources.urls?.[src];
          if (recorded) src = recorded;
          else if (painted) unsupported.push(`${label}: image pixels/response bytes are unavailable`);
        }
      } else if (painted && src) unsupported.push(`${label}: image is not decoded`);
      clone.setAttribute('src', src);
      clone.setAttribute('loading', 'eager');
    }
    if (source instanceof HTMLInputElement) clone.setAttribute('value', source.value);
    if (source instanceof HTMLOptionElement) {
      if (source.selected) clone.setAttribute('selected', '');
      else clone.removeAttribute('selected');
    }
    for (const pseudo of ['::before', '::after']) {
      const style = getComputedStyle(source, pseudo);
      if (style.content && style.content !== 'none' && style.content !== 'normal') {
        pseudos.push(`[data-pagx-capture-id="${id}"]${pseudo}{${styles(style)}}`);
      }
    }
    if (source instanceof HTMLTextAreaElement) clone.textContent = source.value;
    else if (!(source instanceof HTMLCanvasElement)) {
      for (const child of Array.from(source.childNodes)) {
        const copied = copy(child);
        if (copied) clone.appendChild(copied);
      }
    }
    return clone;
  };
  const root = copy(document.documentElement) as HTMLElement;
  let head = root.querySelector('head');
  if (!head) { head = inert.createElement('head'); root.prepend(head); }
  const fonts: string[] = [];
  const fontFamilies = new Set<string>();
  const links: string[] = [];
  const visited = new Set<CSSStyleSheet>();
  const collectFonts = (rules: CSSRuleList, base: string): void => {
    for (const rule of Array.from(rules)) {
      if (rule.type === CSSRule.FONT_FACE_RULE) {
        fonts.push(rewriteUrls(rule.cssText, base));
        fontFamilies.add((rule as CSSFontFaceRule).style.fontFamily.replace(/^["']|["']$/g, ''));
      } else if (rule.type === CSSRule.IMPORT_RULE) {
        const imported = (rule as CSSImportRule).styleSheet;
        if (imported) collectSheet(imported);
      } else if ('cssRules' in rule) collectFonts((rule as CSSGroupingRule).cssRules, base);
    }
  };
  const collectSheet = (sheet: CSSStyleSheet): void => {
    if (visited.has(sheet)) return;
    visited.add(sheet);
    const base = sheet.href || document.baseURI;
    try { collectFonts(sheet.cssRules, base); }
    catch (_) {
      const text = resources.stylesheets?.[base];
      if (text) {
        const parsed = new CSSStyleSheet();
        parsed.replaceSync(text);
        collectFonts(parsed.cssRules, base);
      } else if (sheet.href) links.push(`<link rel="stylesheet" href="${escape(sheet.href)}">`);
    }
  };
  for (const sheet of [...Array.from(document.styleSheets), ...document.adoptedStyleSheets]) collectSheet(sheet);
  for (const face of Array.from(document.fonts)) {
    if (face.status === 'loaded' && !fontFamilies.has(face.family.replace(/^["']|["']$/g, ''))) {
      unsupported.push(`font ${face.family}: runtime font face has no readable CSS definition`);
    }
  }
  head.innerHTML = `<meta charset="UTF-8"><base href="${escape(document.baseURI)}">` +
    links.join('') + `<style>${fonts.concat(pseudos).join('\n').replace(/<\/style/gi, '<\\/style')}</style>`;
  const doctype = document.doctype ? new XMLSerializer().serializeToString(document.doctype)
    : document.compatMode === 'BackCompat' ? '' : '<!DOCTYPE html>';
  return { version: 1, html: doctype + '\n' + root.outerHTML, url: location.href,
    viewport: { width: innerWidth, height: innerHeight }, scroll: [scrollX, scrollY], boxes, unsupported,
    webFonts: Array.from(new Set(Array.from(document.fonts).filter(face => face.status === 'loaded').map(face => face.family))) };
}

/** Rebuild only the captured state. Never runs the source application's scripts. */
export async function renderCapturedPage(
  mirror: EngineWrapper, captured: CapturedPage, options: CaptureOptions = {},
): Promise<CurrentStateResult> {
  if (captured.version !== 1) throw new Error('Unsupported captured-page version');
  if (captured.unsupported.length) throw new StateCaptureError(`Unsupported current-state content: ${captured.unsupported.join('; ')}`);
  const page = await newPage(mirror, { viewport: captured.viewport });
  try {
    // An inert copy must not refetch authenticated/mutable resources under a
    // different session. Uncaptured requests are an error, never a silent fallback.
    const missing = new Set<string>();
    if (mirror.engine === 'playwright') {
      await page.route('**/*', async (route: any) => {
        const url = route.request().url();
        if (/^(data:|about:)/.test(url)) return route.continue();
        missing.add(url); await route.abort();
      });
    } else {
      await page.setRequestInterception(true);
      page.on('request', (request: any) => {
        const url = request.url();
        if (/^(data:|about:)/.test(url)) { void request.continue().catch(() => {}); return; }
        missing.add(url); void request.abort().catch(() => {});
      });
    }
    await page.setContent(captured.html, { waitUntil: 'load', timeout: options.timeoutMs ?? 15000 });
    await page.evaluate(SNAPSHOT_INIT_SCRIPT);
    await page.evaluate(ICON_FONT_INIT_SCRIPT);
    await page.evaluate(async (timeoutMs: number) => {
      let timer: ReturnType<typeof setTimeout> | undefined;
      try {
        await Promise.race([document.fonts.ready, new Promise((_, reject) => {
          timer = setTimeout(() => reject(new Error('Static-copy font readiness timed out')), timeoutMs);
        })]);
      } finally { if (timer) clearTimeout(timer); }
    }, options.timeoutMs ?? 15000);
    if (missing.size) throw new StateCaptureError(`State capture has uncaptured resources: ${Array.from(missing).slice(0, 5).join(', ')}`);
    const differences = await page.evaluate((state: CapturedPage) => {
      for (const box of state.boxes) {
        const node = document.querySelector(`[data-pagx-capture-id="${box.id}"]`);
        if (node) { node.scrollLeft = box.scroll[0]; node.scrollTop = box.scroll[1]; }
      }
      window.scrollTo(state.scroll[0], state.scroll[1]);
      const errors: string[] = [];
      for (const box of state.boxes) {
        const node = document.querySelector(`[data-pagx-capture-id="${box.id}"]`);
        if (!node) { errors.push(`${box.tag}: missing from static copy`); continue; }
        const rect = node.getBoundingClientRect();
        if ([rect.x, rect.y, rect.width, rect.height].some((v, i) => Math.abs(v - box.rect[i]) > 1)) {
          errors.push(`${box.tag}: static copy changed layout (${box.rect.join(',')} -> ${[rect.x, rect.y, rect.width, rect.height].join(',')})`);
        }
      }
      return errors;
    }, captured);
    if (differences.length) throw new StateCaptureError(`State capture could not preserve geometry: ${differences.slice(0, 5).join('; ')}`);
    await page.evaluate(inlineExternalImages, {});
    await page.evaluate(inlineCanvases);
    if (options.inlineIconFonts !== false) await inlineIconFontsOnPage(page, {});
    await page.evaluate(materializeDecorativePseudoElements);
    const snapshot = await page.evaluate((extent: string) => {
      const api = (window as unknown as { __pagxSnapshot: { takeSnapshot: (opts: object) =>
        { html: string; width: number; height: number } } }).__pagxSnapshot;
      return api.takeSnapshot({ mode: 'current-state', extent });
    }, options.extent || 'viewport');
    snapshot.html = snapshot.html.replace(/ data-pagx-capture-id="\d+"/g, '');
    return { ...snapshot, warnings: captured.webFonts?.length ? [
      'Web fonts require native font fallbacks; use pagx import --html-embed-fonts for a portable PAGX file.',
    ] : [], url: captured.url };
  } finally { await page.close(); }
}

/** Captures an already-open page without navigating, closing, or mutating it. */
export async function captureCurrentPage(
  page: Page, options: CaptureOptions & { engine?: EngineName; mirror?: EngineWrapper } = {},
): Promise<CurrentStateResult> {
  const sourceBrowser = typeof page.browser === 'function' ? page.browser() : page.context().browser();
  if (options.mirror?.browser === sourceBrowser) {
    throw new Error('The static-copy renderer must use a separate browser to preserve source focus');
  }
  const resources: CaptureResources = { urls: {}, stylesheets: {} };
  for (const [url, entry] of options.resources || []) {
    if (entry.type === 'stylesheet') resources.stylesheets![url] = entry.buffer.toString('utf8');
    else resources.urls![url] = `data:${entry.contentType};base64,${entry.buffer.toString('base64')}`;
  }
  const captured = await page.evaluate(collectCurrentState, resources) as CapturedPage;
  // If a fallback is allowed, acquire its pixels next to the state collection,
  // before waiting for an isolated renderer that may discover an unsupported case.
  const canRasterize = options.unsupported === 'rasterize' && options.extent !== 'document';
  const png = canRasterize ? Buffer.from(await page.screenshot({ type: 'png', fullPage: false })) : null;
  const raster = (warnings: string[]): CurrentStateResult => {
    const { width, height } = captured.viewport;
    return { width, height, url: captured.url, warnings,
      html: `<!DOCTYPE html><html><body data-pagx-capture-mode="current-state" style="margin:0;width:${width}px;height:${height}px"><img style="display:block;width:${width}px;height:${height}px" src="data:image/png;base64,${png!.toString('base64')}"/></body></html>` };
  };
  if (captured.unsupported.length) {
    if (!png) throw new StateCaptureError(`Unsupported current-state content: ${captured.unsupported.join('; ')}`);
    // Whole-viewport fallback preserves compositing. A cropped element bitmap
    // would also capture its backdrop and could paint overlapping content twice.
    return raster(captured.unsupported);
  }
  const mirror = options.mirror || await launchBrowser({ engine: options.engine });
  try { return await renderCapturedPage(mirror, captured, options); }
  catch (error) {
    if (png && error instanceof StateCaptureError) return raster([error.message]);
    throw error;
  }
  finally { if (!options.mirror) await mirror.browser.close(); }
}
