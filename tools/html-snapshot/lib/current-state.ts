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
  /** Capture limitations, retained in version-1 payloads and returned as warnings. */
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
  const seenWarnings = new Set<string>();
  const warn = (message: string): void => {
    if (!seenWarnings.has(message)) { seenWarnings.add(message); unsupported.push(message); }
  };
  const pseudos: string[] = [];
  const escape = (value: string) => value.replace(/&/g, '&amp;').replace(/"/g, '&quot;')
    .replace(/</g, '&lt;').replace(/>/g, '&gt;');
  const documentUrl = new URL(document.URL);
  documentUrl.hash = '';
  const resolveResource = (raw: string, base = document.baseURI): string | null => {
    if (raw.startsWith('#') || /^data:/i.test(raw)) return raw;
    try {
      const url = new URL(raw, base);
      const fragment = url.hash;
      url.hash = '';
      if (fragment && url.href === documentUrl.href) return fragment;
      const captured = resources.urls?.[url.href];
      if (captured && /^data:/i.test(captured)) return captured + fragment;
      warn(`Uncaptured resource omitted: ${url.href}`);
    } catch (_) { warn(`Invalid resource URL omitted: ${raw}`); }
    return null;
  };
  const rewriteUrls = (value: string, base = document.baseURI, onMissing?: () => void): string => value.replace(
    /url\(\s*(?:"((?:\\.|[^"\\])*)"|'((?:\\.|[^'\\])*)'|([^)]*))\s*\)/gi,
    (_original, double, single, unquoted) => {
      const raw = String(double ?? single ?? unquoted).trim().replace(/\\(["'\\()])/g, '$1');
      const result = resolveResource(raw, base);
      if (result === null) { onMissing?.(); return 'none'; }
      return `url("${result.replace(/"/g, '%22')}")`;
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
    if (['script', 'style', 'link', 'meta', 'base', 'noscript', 'source', 'track'].includes(tag)) return null;
    const computed = getComputedStyle(source);
    const rect = source.getBoundingClientRect();
    const painted = computed.display !== 'none' && computed.visibility !== 'hidden' &&
      Number(computed.opacity) !== 0 && rect.width > 0 && rect.height > 0;
    const label = `${tag}${source.id ? `#${source.id}` : ''}`;
    if (painted && (source.shadowRoot || tag.includes('-') ||
        ['iframe', 'object', 'embed', 'video', 'audio', 'details', 'dialog', 'meter', 'progress'].includes(tag))) {
      warn(`${label}: runtime content may be omitted from static capture`);
    }
    if (painted && source.matches(':modal, :popover-open')) {
      warn(`${label}: browser top-layer positioning may differ in static capture`);
    }
    if (painted && source instanceof HTMLInputElement &&
        !['text', 'search', 'email', 'url', 'tel', 'number', 'hidden'].includes(source.type)) {
      warn(`${label}: ${source.type} control appearance may differ in static capture`);
    }
    // Embedded documents/media must remain inert, including data: and srcdoc content.
    // Keep their painted box and light-DOM children, without starting another runtime.
    let placeholder = ['iframe', 'object', 'embed', 'video', 'audio'].includes(tag);
    let clone = placeholder ? inert.createElement('div') : inert.importNode(source, false) as Element;
    if (source instanceof HTMLCanvasElement) {
      try {
        const image = inert.createElement('img');
        image.src = source.toDataURL('image/png');
        if (image.src === 'data:,') throw new Error('Empty canvas');
        clone = image;
      } catch (_) {
        if (painted) warn(`${label}: canvas pixels are not readable; pixels omitted`);
        clone = inert.createElement('div');
        placeholder = true;
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
    if (placeholder && computed.display === 'inline') {
      (clone as HTMLElement).style.setProperty('display', 'inline-block', 'important');
    }
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
          src = resolveResource(src) || '';
          if (!src && painted) warn(`${label}: image pixels/response bytes are unavailable; image omitted`);
        }
      } else {
        if (painted && src) warn(`${label}: image is not decoded; image omitted`);
        src = '';
      }
      if (src) clone.setAttribute('src', src);
      else clone.removeAttribute('src');
      clone.setAttribute('loading', 'eager');
    }
    if (source instanceof HTMLInputElement) {
      clone.setAttribute('value', source.value);
      clone.toggleAttribute('checked', source.checked);
      if (source.indeterminate) warn(`${label}: indeterminate control state is omitted`);
      if (source.type === 'image' && source.src) {
        const src = resolveResource(source.src);
        if (src) clone.setAttribute('src', src);
        else clone.removeAttribute('src');
      }
    }
    if (source.namespaceURI === 'http://www.w3.org/2000/svg' && ['image', 'use', 'feImage'].includes(tag)) {
      for (const attr of ['href', 'xlink:href']) {
        const raw = source.getAttribute(attr);
        if (raw) {
          const value = resolveResource(raw);
          if (value) clone.setAttributeNS(attr === 'xlink:href' ? 'http://www.w3.org/1999/xlink' : null, attr, value);
          else clone.removeAttribute(attr);
        }
      }
    }
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
  const visited = new Set<CSSStyleSheet>();
  // Split font src alternatives without splitting commas inside URLs or quoted names.
  const fontSources = (value: string): string[] => {
    const result: string[] = [];
    let quote = '', depth = 0, start = 0;
    for (let i = 0; i < value.length; i++) {
      const c = value[i];
      if (c === '\\') { i++; continue; }
      if (quote) { if (c === quote) quote = ''; }
      else if (c === '"' || c === "'") quote = c;
      else if (c === '(') depth++;
      else if (c === ')') depth--;
      else if (c === ',' && depth === 0) { result.push(value.slice(start, i)); start = i + 1; }
    }
    result.push(value.slice(start));
    return result;
  };
  const collectFonts = (rules: CSSRuleList, base: string): void => {
    for (const rule of Array.from(rules)) {
      if (rule.type === CSSRule.FONT_FACE_RULE) {
        const style = (rule as CSSFontFaceRule).style;
        const sources = fontSources(style.getPropertyValue('src')).flatMap(source => {
          let missing = false;
          const rewritten = rewriteUrls(source, base, () => { missing = true; });
          return missing || !rewritten.trim() ? [] : [rewritten];
        });
        if (sources.length) {
          fonts.push(`@font-face{${Array.from(style).map(key =>
            `${key}:${key === 'src' ? sources.join(',') : style.getPropertyValue(key)}`).join(';')}}`);
          fontFamilies.add(style.fontFamily.replace(/^["']|["']$/g, ''));
        } else warn(`font ${style.fontFamily}: no captured source; using font fallback`);
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
      } else if (sheet.href) warn(`Stylesheet font definitions unavailable: ${sheet.href}`);
    }
  };
  for (const sheet of [...Array.from(document.styleSheets), ...document.adoptedStyleSheets]) collectSheet(sheet);
  for (const face of Array.from(document.fonts)) {
    if (face.status === 'error') warn(`font ${face.family}: failed to load; using font fallback`);
    if (face.status === 'loaded' && !fontFamilies.has(face.family.replace(/^["']|["']$/g, ''))) {
      warn(`font ${face.family}: runtime font face has no captured CSS definition; using font fallback`);
    }
  }
  head.innerHTML = `<meta charset="UTF-8"><base href="${escape(document.baseURI)}">` +
    `<style>${fonts.concat(pseudos).join('\n').replace(/<\/style/gi, '<\\/style')}</style>`;
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
  const warnings = [...captured.unsupported];
  const page = await newPage(mirror, { viewport: captured.viewport });
  try {
    // An inert copy must not refetch authenticated/mutable resources under a
    // different session. Block any remaining external requests and report them.
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
    // Older version-1 collectors may have retained embedded documents. Keep the
    // renderer inert even when accepting those payloads with capture warnings.
    const html = await page.evaluate((source: string) => {
      const doc = new DOMParser().parseFromString(source, 'text/html');
      const policy = doc.createElement('meta');
      policy.httpEquiv = 'Content-Security-Policy';
      policy.content = "script-src 'none'; frame-src 'none'; object-src 'none'";
      doc.head.prepend(policy);
      return (doc.doctype ? new XMLSerializer().serializeToString(doc.doctype) : '') + doc.documentElement.outerHTML;
    }, captured.html);
    await page.setContent(html, { waitUntil: 'load', timeout: options.timeoutMs ?? 15000 });
    await page.evaluate(SNAPSHOT_INIT_SCRIPT);
    await page.evaluate(ICON_FONT_INIT_SCRIPT);
    const fontWarnings: string[] = await page.evaluate(async (timeoutMs: number) => {
      let timer: ReturnType<typeof setTimeout> | undefined;
      try {
        await Promise.race([document.fonts.ready, new Promise((_, reject) => {
          timer = setTimeout(() => reject(new Error('Static-copy font readiness timed out')), timeoutMs);
        })]);
      } finally { if (timer) clearTimeout(timer); }
      return Array.from(document.fonts).filter(face => face.status === 'error')
        .map(face => `font ${face.family}: failed to load; using font fallback`);
    }, options.timeoutMs ?? 15000);
    for (const warning of fontWarnings) warnings.push(warning);
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
    for (const difference of differences) warnings.push(difference);
    // Do not pass blocked URLs from older payloads on to the native importer.
    const resourceWarnings: string[] = await page.evaluate(() => {
      const warnings = new Set<string>();
      const base = new URL(document.baseURI);
      base.hash = '';
      const local = (raw: string): boolean => {
        if (raw.startsWith('#') || /^data:/i.test(raw)) return true;
        try {
          const url = new URL(raw, document.baseURI);
          const fragment = url.hash;
          url.hash = '';
          if (fragment && url.href === base.href) return true;
          warnings.add(`Uncaptured resource omitted: ${url.href}`);
        } catch (_) { warnings.add(`Invalid resource URL omitted: ${raw}`); }
        return false;
      };
      const cleanUrls = (value: string): string => value.replace(
        /url\(\s*(?:"((?:\\.|[^"\\])*)"|'((?:\\.|[^'\\])*)'|([^)]*))\s*\)/gi,
        (token, double, single, unquoted) => local(String(double ?? single ?? unquoted).trim()
          .replace(/\\(["'\\()])/g, '$1')) ? token : 'none');
      // Pseudo-elements inherit their URLs from stylesheet rules, rather than
      // the element's inline/computed style. Clean those before materialization.
      const cleanRules = (rules: CSSRuleList): void => {
        for (const rule of Array.from(rules)) {
          if (rule.type === CSSRule.STYLE_RULE) {
            const style = (rule as CSSStyleRule).style;
            for (const key of Array.from(style)) {
              const value = style.getPropertyValue(key), clean = cleanUrls(value);
              if (clean !== value) style.setProperty(key, clean, style.getPropertyPriority(key));
            }
          }
          if ('cssRules' in rule) cleanRules((rule as CSSGroupingRule).cssRules);
        }
      };
      for (const sheet of Array.from(document.styleSheets)) {
        try { cleanRules(sheet.cssRules); }
        catch (error) {
          if (!(error instanceof DOMException) || error.name !== 'SecurityError') throw error;
        }
      }
      for (const el of Array.from(document.querySelectorAll('*'))) {
        const style = getComputedStyle(el);
        for (const key of Array.from(style)) {
          const value = style.getPropertyValue(key);
          if (!/url\(/i.test(value)) continue;
          const clean = cleanUrls(value);
          if (clean !== value && 'style' in el) (el as HTMLElement).style.setProperty(key, clean, 'important');
        }
        const attrs = el instanceof HTMLImageElement || el instanceof HTMLInputElement ? ['src']
          : el.namespaceURI === 'http://www.w3.org/2000/svg' && ['image', 'use', 'feImage'].includes(el.localName)
            ? ['href', 'xlink:href'] : [];
        for (const attr of attrs) {
          const value = el.getAttribute(attr);
          if (value && !local(value)) el.removeAttribute(attr);
        }
        if (el instanceof HTMLImageElement && el.getAttribute('src') && el.complete && !el.naturalWidth) {
          el.removeAttribute('src');
          warnings.add(`img${el.id ? `#${el.id}` : ''}: image is not decoded; image omitted`);
        }
      }
      return Array.from(warnings);
    });
    for (const warning of resourceWarnings) warnings.push(warning);
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
    for (const url of missing) warnings.push(`Uncaptured resource omitted: ${url}`);
    if (captured.webFonts?.length) warnings.push(
      'Web fonts require native font fallbacks; use pagx import --html-embed-fonts for a portable PAGX file.');
    return { ...snapshot, warnings: Array.from(new Set(warnings)), url: captured.url };
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
  const mirror = options.mirror || await launchBrowser({ engine: options.engine });
  try { return await renderCapturedPage(mirror, captured, options); }
  finally { if (!options.mirror) await mirror.browser.close(); }
}
