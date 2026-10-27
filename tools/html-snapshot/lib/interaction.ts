// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0.

import { openAndSettlePage } from './page-loader';
import { captureCurrentPage, type CaptureOptions, type CurrentStateResult } from './current-state';
import { launchBrowser, type EngineWrapper, type Page, type CookieParam } from './browser-engine';
import { StateResources } from './state-resources';
import { saveDownloadedFonts } from './font-download';
import { saveDownloadedImages } from './image-download';

export interface Target {
  css?: string;
  testId?: string;
  role?: string;
  name?: string;
  frame?: string;
}

export interface Step {
  type: 'click' | 'fill' | 'type' | 'press' | 'hover' | 'focus' | 'check' | 'select' |
    'scroll' | 'waitFor' | 'capture';
  target?: Target;
  value?: string | boolean;
  key?: string;
  x?: number;
  y?: number;
  name?: string;
  state?: 'visible' | 'hidden' | 'attached' | 'detached';
  attribute?: { name: string; value: string };
  text?: string;
  url?: string;
  extent?: CaptureOptions['extent'];
  unsupported?: CaptureOptions['unsupported'];
  timeoutMs?: number;
}

export interface Scenario {
  version: 1;
  viewport: { width: number; height: number };
  timeoutMs: number;
  stepTimeoutMs: number;
  stableMs: number;
  steps: Step[];
}

export interface Checkpoint extends CurrentStateResult { name: string }

function object(value: unknown, label: string): Record<string, unknown> {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value as Record<string, unknown>;
}

function keys(value: Record<string, unknown>, allowed: string[], label: string): void {
  for (const key of Object.keys(value)) if (!allowed.includes(key)) throw new Error(`${label}: unknown field '${key}'`);
}

function string(value: unknown, label: string): string {
  if (typeof value !== 'string' || !value.length) throw new Error(`${label} must be a non-empty string`);
  return value;
}

function number(value: unknown, fallback: number, label: string, min: number, max: number): number {
  const result = value === undefined ? fallback : value;
  if (typeof result !== 'number' || !Number.isFinite(result) || result < min || result > max) {
    throw new Error(`${label} must be between ${min} and ${max}`);
  }
  return result;
}

/** Reject malformed/unknown options before opening a page or executing actions. */
export function parseScenario(input: unknown): Scenario {
  const raw = object(input, 'scenario');
  keys(raw, ['version', 'viewport', 'timeoutMs', 'stepTimeoutMs', 'stableMs', 'steps'], 'scenario');
  if (raw.version !== 1) throw new Error('scenario.version must be 1');
  const viewport = raw.viewport === undefined ? {} : object(raw.viewport, 'viewport');
  keys(viewport, ['width', 'height'], 'viewport');
  if (!Array.isArray(raw.steps) || !raw.steps.length || raw.steps.length > 256) {
    throw new Error('scenario.steps must contain 1–256 steps');
  }
  const fields: Record<Step['type'], string[]> = {
    click: ['target', 'url'], fill: ['target', 'value'], type: ['target', 'value'],
    press: ['target', 'key', 'url'], hover: ['target'], focus: ['target'],
    check: ['target', 'value'], select: ['target', 'value'], scroll: ['target', 'x', 'y'],
    waitFor: ['target', 'state', 'attribute', 'text', 'url'],
    capture: ['name', 'extent', 'unsupported'],
  };
  const names = new Set<string>();
  for (const key of ['width', 'height']) {
    if (viewport[key] !== undefined && !Number.isInteger(viewport[key])) throw new Error(`viewport.${key} must be an integer`);
  }
  const steps: Step[] = raw.steps.map((value, index) => {
    const label = `step ${index + 1}`;
    const step = object(value, label);
    const type = string(step.type, `${label}.type`) as Step['type'];
    if (!Object.hasOwn(fields, type)) throw new Error(`${label}: unsupported action '${type}'`);
    keys(step, ['type', 'timeoutMs', ...fields[type]], label);
    if (step.timeoutMs !== undefined) number(step.timeoutMs, 0, `${label}.timeoutMs`, 1, 300000);
    if (step.target !== undefined) {
      const target = object(step.target, `${label}.target`);
      keys(target, ['css', 'testId', 'role', 'name', 'frame'], `${label}.target`);
      if (['css', 'testId', 'role'].filter(key => target[key] !== undefined).length !== 1) {
        throw new Error(`${label}.target must specify exactly one of css, testId, role`);
      }
      for (const [key, val] of Object.entries(target)) string(val, `${label}.target.${key}`);
      if (target.name !== undefined && !target.role) throw new Error(`${label}: name requires role`);
    } else if (!['capture', 'scroll'].includes(type) && !(type === 'waitFor' && step.url)) {
      throw new Error(`${label}: target is required`);
    }
    if (['fill', 'type', 'select'].includes(type) && typeof step.value !== 'string') {
      throw new Error(`${label}.value must be a string`);
    }
    if (type === 'check' && typeof step.value !== 'boolean') throw new Error(`${label}.value must be boolean`);
    if (type === 'press') string(step.key, `${label}.key`);
    if (step.url !== undefined) string(step.url, `${label}.url`);
    if (type === 'scroll') {
      if (step.x === undefined && step.y === undefined) throw new Error(`${label}: scroll requires x or y`);
      for (const key of ['x', 'y']) if (step[key] !== undefined) number(step[key], 0, label, -1e7, 1e7);
    }
    if (type === 'waitFor') {
      if ([step.state, step.attribute, step.text, step.url].filter(v => v !== undefined).length !== 1) {
        throw new Error(`${label}: waitFor requires exactly one condition`);
      }
      if (step.state !== undefined && !['visible', 'hidden', 'attached', 'detached'].includes(String(step.state))) {
        throw new Error(`${label}: invalid visibility state`);
      }
      if (step.text !== undefined && typeof step.text !== 'string') throw new Error(`${label}.text must be a string`);
      if (step.attribute !== undefined) {
        const attr = object(step.attribute, `${label}.attribute`);
        keys(attr, ['name', 'value'], `${label}.attribute`);
        string(attr.name, `${label}.attribute.name`);
        if (typeof attr.value !== 'string') throw new Error(`${label}.attribute.value must be a string`);
      }
    }
    if (type === 'capture') {
      const name = string(step.name, `${label}.name`);
      if (!/^[a-zA-Z0-9][a-zA-Z0-9_-]{0,79}$/.test(name)) throw new Error(`${label}: invalid checkpoint name`);
      if (names.has(name)) throw new Error(`${label}: duplicate checkpoint '${name}'`);
      names.add(name);
      if (step.extent !== undefined && !['viewport', 'document'].includes(String(step.extent))) throw new Error(`${label}: invalid extent`);
      if (step.unsupported !== undefined && !['error', 'rasterize'].includes(String(step.unsupported))) throw new Error(`${label}: invalid unsupported policy`);
    }
    return { ...step } as unknown as Step;
  });
  if (!names.size || names.size > 32) throw new Error('scenario requires 1–32 named capture steps');
  return { version: 1, steps, viewport: {
    width: number(viewport.width, 1400, 'viewport.width', 1, 16384),
    height: number(viewport.height, 900, 'viewport.height', 1, 16384),
  }, timeoutMs: number(raw.timeoutMs, 120000, 'timeoutMs', 1, 600000),
  stepTimeoutMs: number(raw.stepTimeoutMs, 15000, 'stepTimeoutMs', 1, 300000),
  stableMs: number(raw.stableMs, 150, 'stableMs', 0, 5000) };
}

/** Single-output entry points must never silently pick one of several states. */
export function selectCheckpoint(scenario: Scenario, checkpoint?: string): string {
  const names = scenario.steps.filter(s => s.type === 'capture').map(s => s.name!);
  if (checkpoint) {
    if (!names.includes(checkpoint)) throw new Error(`Unknown checkpoint '${checkpoint}'`);
    return checkpoint;
  }
  if (names.length !== 1) throw new Error('Multiple checkpoints: select one with the checkpoint option, or use pagx import --output-dir for all states');
  return names[0];
}

const sleep = (ms: number) => new Promise<void>(resolve => setTimeout(resolve, ms));

function selector(target: Target): string {
  if (target.css) return target.css;
  // JSON's quoted string syntax is sufficient for this CSS attribute value,
  // with control characters represented as CSS hex escapes.
  if (target.testId) return `[data-testid="${target.testId.replace(/[\\"\n\r\f]/g,
    c => `\\${c.codePointAt(0)!.toString(16)} `)}"]`;
  throw new Error("Role locators require the 'playwright' browser engine");
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function scopeFor(page: Page, engine: string, target: Target): Promise<any> {
  if (!target.frame) return page;
  if (engine === 'playwright') {
    if (await page.locator(target.frame).count() !== 1) throw new Error('Frame locator must match exactly one frame');
    return page.frameLocator(target.frame);
  }
  const matches = await page.$$(target.frame);
  try {
    if (matches.length !== 1) throw new Error('Frame locator must match exactly one frame');
    const frame = await matches[0].contentFrame();
    if (!frame) throw new Error('Target is not an attached frame');
    return frame;
  } finally { await Promise.all(matches.map((m: { dispose(): Promise<void> }) => m.dispose())); }
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
function playwrightLocator(scope: any, target: Target): any {
  if (target.role) return scope.getByRole(target.role, { name: target.name, exact: true });
  return scope.locator(selector(target));
}

/** Poll without retaining element handles across application rerenders. */
async function waitCondition(page: Page, engine: string, step: Step, timeoutMs: number): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  const expectedUrl = step.url ? new URL(step.url, page.url()).href : '';
  do {
    if (step.url) {
      if (page.url() === expectedUrl) return;
    } else {
      const target = step.target!;
      const scope = await scopeFor(page, engine, target);
      const handles = engine === 'playwright'
        ? await playwrightLocator(scope, target).elementHandles() : await scope.$$(selector(target));
      try {
        if (handles.length > 1) throw new Error('Locator matched multiple elements');
        if (!handles.length) {
          if (step.state === 'detached' || step.state === 'hidden') return;
        } else {
          const matches = await handles[0].evaluate((el: Element, condition: Step) => {
            const cs = getComputedStyle(el);
            const r = el.getBoundingClientRect();
            const visible = cs.display !== 'none' && cs.visibility !== 'hidden' && Number(cs.opacity) > 0 && r.width > 0 && r.height > 0;
            if (condition.state === 'attached') return true;
            if (condition.state === 'detached') return false;
            if (condition.state === 'visible') return visible;
            if (condition.state === 'hidden') return !visible;
            if (condition.attribute) return el.getAttribute(condition.attribute.name) === condition.attribute.value;
            return el.textContent === condition.text;
          }, step);
          if (matches) return;
        }
      } finally { await Promise.all(handles.map((h: { dispose(): Promise<void> }) => h.dispose())); }
    }
    await sleep(Math.min(50, Math.max(0, deadline - Date.now())));
  } while (Date.now() < deadline);
  throw new Error(`Condition timed out after ${timeoutMs} ms`);
}

async function perform(page: Page, engine: string, step: Step, timeoutMs: number): Promise<void> {
  if (step.type === 'waitFor') return waitCondition(page, engine, step, timeoutMs);
  if (step.type === 'scroll' && !step.target) {
    await page.evaluate((s: Step) => window.scrollTo({ left: s.x ?? scrollX, top: s.y ?? scrollY, behavior: 'instant' }), step);
    return;
  }
  const target = step.target!;
  await waitCondition(page, engine, { type: 'waitFor', target, state: 'attached' }, timeoutMs);
  const scope = await scopeFor(page, engine, target);
  if (engine === 'playwright') {
    const locator = playwrightLocator(scope, target);
    const opts = { timeout: timeoutMs };
    switch (step.type) {
      case 'click': await locator.click(opts); break;
      case 'fill': await locator.fill(step.value, opts); break;
      case 'type': await locator.pressSequentially(step.value, opts); break;
      case 'press': await locator.press(step.key, opts); break;
      case 'hover': await locator.hover(opts); break;
      case 'focus': await locator.focus(opts); break;
      case 'check': await locator.setChecked(step.value, opts); break;
      case 'select': {
        const selected = await locator.selectOption(step.value, opts);
        if (!selected.includes(step.value)) throw new Error('Requested option was not selected');
        break;
      }
      case 'scroll': await locator.evaluate((el: Element, s: Step) => el.scrollTo({
        left: s.x ?? el.scrollLeft, top: s.y ?? el.scrollTop, behavior: 'instant',
      }), step); break;
    }
  } else {
    const css = selector(target);
    const locator = scope.locator(css).setTimeout(timeoutMs);
    if (step.type === 'click') await locator.click();
    else if (step.type === 'fill') await locator.fill(step.value);
    else if (step.type === 'hover') await locator.hover();
    else {
      const handle = await scope.$(css);
      if (!handle) throw new Error('Element detached before action');
      try {
        switch (step.type) {
          case 'type': await handle.type(step.value); break;
          case 'press': await handle.press(step.key); break;
          case 'focus': await handle.focus(); break;
          case 'select': {
            const selected = await handle.select(step.value);
            if (!selected.includes(step.value)) throw new Error('Requested option was not selected');
            break;
          }
          case 'check': {
            const checked = await handle.evaluate((el: HTMLInputElement) => {
              if (!(el instanceof HTMLInputElement) || !['checkbox', 'radio'].includes(el.type)) throw new Error('check requires a checkbox or radio');
              return el.checked;
            });
            if (checked !== step.value) await locator.click();
            if (await scope.$eval(css, (el: HTMLInputElement) => el.checked) !== step.value) throw new Error('Checked state did not change');
            break;
          }
          case 'scroll': await handle.evaluate((el: Element, s: Step) => el.scrollTo({
            left: s.x ?? el.scrollLeft, top: s.y ?? el.scrollTop, behavior: 'instant',
          }), step); break;
        }
      } finally { await handle.dispose(); }
    }
  }
}

/** Resource and layout readiness is bounded; explicit waitFor steps express business readiness. */
export async function waitForCaptureReady(page: Page, timeoutMs: number, stableMs: number,
  extent = 'viewport'): Promise<void> {
  await page.evaluate(async (cfg: { timeoutMs: number; stableMs: number; extent: string }) => {
    const deadline = performance.now() + cfg.timeoutMs;
    let last = '', since = performance.now();
    while (performance.now() < deadline) {
      const images = Array.from(document.images).filter(img => {
        const rect = img.getBoundingClientRect();
        const c = getComputedStyle(img);
        return c.display !== 'none' && c.visibility !== 'hidden' && rect.width > 0 && rect.height > 0 &&
          (cfg.extent === 'document' || (rect.bottom > 0 && rect.right > 0 && rect.top < innerHeight && rect.left < innerWidth));
      });
      const resourceReady = document.fonts.status === 'loaded' && images.every(img => img.complete);
      const nodes = Array.from(document.body.querySelectorAll('*')).filter(el => {
        const r = el.getBoundingClientRect();
        return cfg.extent === 'document' || (r.bottom > 0 && r.right > 0 && r.top < innerHeight && r.left < innerWidth);
      });
      const fingerprint = JSON.stringify(nodes.map(el => {
        const r = el.getBoundingClientRect(), c = getComputedStyle(el);
        return [r.x, r.y, r.width, r.height, c.opacity, c.visibility, c.transform,
          c.backgroundColor, c.backgroundImage, c.color, c.filter, c.boxShadow, c.clipPath,
          el.childElementCount ? '' : el.textContent];
      }));
      if (fingerprint !== last || !resourceReady) { since = performance.now(); last = fingerprint; }
      if (resourceReady && performance.now() - since >= cfg.stableMs) {
        if (images.some(img => !img.naturalWidth)) throw new Error('Visible image failed to load');
        let timer: ReturnType<typeof setTimeout> | undefined;
        try {
          await Promise.race([Promise.all(images.map(img => img.decode())), new Promise((_, reject) => {
            timer = setTimeout(() => reject(new Error('Image decode timed out')), Math.max(1, deadline - performance.now()));
          })]);
        } finally { if (timer) clearTimeout(timer); }
        return;
      }
      await new Promise(resolve => setTimeout(resolve, 30));
    }
    throw new Error('Capture readiness timed out; add an explicit waitFor or set stableMs: 0 for a moving state');
  }, { timeoutMs, stableMs, extent });
}

export interface RunScenarioOptions {
  checkpoint?: string;
  cookies?: CookieParam[];
  headers?: Array<[string, string]>;
  inlineIconFonts?: boolean;
  downloadFonts?: boolean;
  fontDir?: string;
  downloadImages?: boolean;
  imageDir?: string;
  log?: (message: string) => void;
  onCheckpoint?: (result: Checkpoint) => Promise<void>;
}

/** One isolated context per scenario; all steps/checkpoints share its page. */
export async function runScenario(
  engineHandle: EngineWrapper, url: string, input: unknown, options: RunScenarioOptions = {},
): Promise<Checkpoint[]> {
  const scenario = parseScenario(input);
  if (options.downloadFonts && !options.fontDir) throw new Error('downloadFonts requires fontDir');
  if (options.downloadImages && !options.imageDir) throw new Error('downloadImages requires imageDir');
  if (options.checkpoint && !scenario.steps.some(s => s.type === 'capture' && s.name === options.checkpoint)) {
    throw new Error(`Unknown checkpoint '${options.checkpoint}'`);
  }
  const { browser, engine } = engineHandle;
  const resourceCapture = new StateResources(engine, options.log);
  if (engine !== 'playwright' && scenario.steps.some(s => s.target?.role)) {
    throw new Error("Role locators require the 'playwright' browser engine");
  }
  const context = engine === 'playwright' ? await browser.newContext() : await browser.createBrowserContext();
  let mirror: EngineWrapper | undefined;
  let timer: ReturnType<typeof setTimeout> | undefined;
  let cancelled = false;
  const deadline = Date.now() + scenario.timeoutMs;
  const remaining = (step?: Step) => Math.max(1, Math.min(step?.timeoutMs ?? scenario.stepTimeoutMs, deadline - Date.now()));
  const work = async () => {
    const page = await openAndSettlePage({ engine, browser: context }, url, {
      viewportWidth: scenario.viewport.width, viewportHeight: scenario.viewport.height,
      waitMs: 0, reducedMotion: false, autoScroll: false,
      cookies: options.cookies, headers: options.headers,
      onResponse: resourceCapture.onResponse,
    });
    const results: Checkpoint[] = [];
    for (let index = 0; index < scenario.steps.length; index++) {
      if (cancelled || Date.now() >= deadline) throw new Error('Scenario timed out');
      const step = scenario.steps[index];
      let stepTimer: ReturnType<typeof setTimeout> | undefined;
      options.log?.(`step ${index + 1}/${scenario.steps.length}: ${step.type}${step.name ? ` ${step.name}` : ''}`);
      try {
        const task = (async (): Promise<boolean> => {
          if (step.type === 'capture') {
            if (options.checkpoint && step.name !== options.checkpoint) return false;
            await waitForCaptureReady(page, remaining(step), scenario.stableMs, step.extent);
            const resources = await resourceCapture.snapshot(remaining(step));
            if (cancelled) throw new Error('Scenario timed out');
            if (!mirror) mirror = await launchBrowser({ engine });
            if (cancelled) {
              await mirror.browser.close();
              throw new Error('Scenario timed out');
            }
            const result: Checkpoint = { ...await captureCurrentPage(page, {
              mirror, engine, extent: step.extent, unsupported: step.unsupported, timeoutMs: remaining(step),
              inlineIconFonts: options.inlineIconFonts,
              resources,
            }), name: step.name! };
            if (options.downloadFonts) {
              const fonts = new Map(Array.from(resources).filter(([, value]) => value.type === 'font')
                .map(([url, value]) => [url, value.buffer]));
              result.fonts = await saveDownloadedFonts(fonts, options.fontDir!, warning => result.warnings.push(warning));
            }
            if (options.downloadImages) {
              const urls = Array.from(new Set(result.html.match(/data:image\/[^;,"'\s]+;base64,[a-zA-Z0-9+/=]+/g) || []));
              const images = new Map(urls.map((url, index) => [`https://capture.invalid/image${index}`, {
                buffer: Buffer.from(url.slice(url.indexOf(',') + 1), 'base64'), contentType: url.slice(5, url.indexOf(';')),
              }]));
              const saved = await saveDownloadedImages(images, options.imageDir!, warning => result.warnings.push(warning));
              for (const image of saved) {
                const index = Number(new URL(image.url).pathname.slice('/image'.length));
                const escaped = image.path.replace(/&/g, '&amp;').replace(/"/g, '&quot;');
                result.html = result.html.split(urls[index]).join(escaped);
              }
            }
            if (cancelled) throw new Error('Scenario timed out');
            results.push(result);
            if (options.onCheckpoint) await options.onCheckpoint(result);
            if (options.checkpoint) return true;
          } else {
            // The URL waiter starts before the input; no implicit click retry on
            // navigation/readiness timeout. Each input is sent at most once here.
            const waitUrl = step.url && step.type !== 'waitFor'
              ? waitCondition(page, engine, { type: 'waitFor', url: new URL(step.url, page.url()).href }, remaining(step)) : null;
            if (waitUrl) {
              const done = await Promise.allSettled([waitUrl, perform(page, engine, step, remaining(step))]);
              for (const item of done) if (item.status === 'rejected') throw item.reason;
            } else await perform(page, engine, step, remaining(step));
          }
          return false;
        })();
        const stop = await Promise.race([task, new Promise<never>((_, reject) => {
          const timeout = remaining(step);
          stepTimer = setTimeout(() => {
            cancelled = true;
            reject(new Error(`Step timed out after ${timeout} ms`));
          }, timeout);
        })]);
        if (stop) break;
      } catch (err) {
        throw new Error(`Scenario step ${index + 1} (${step.type}${step.name ? ` ${step.name}` : ''}): ${(err as Error).message}`);
      } finally { if (stepTimer) clearTimeout(stepTimer); }
    }
    return results;
  };
  const pending = work();
  try {
    return await Promise.race([pending, new Promise<never>((_, reject) => {
      timer = setTimeout(() => { cancelled = true; reject(new Error(`Scenario timed out after ${scenario.timeoutMs} ms`)); }, scenario.timeoutMs);
    })]);
  } finally {
    cancelled = true;
    if (timer) clearTimeout(timer);
    await context.close();
    if (mirror) await mirror.browser.close();
    // A consumer callback need not be cancellable. Do not let it defeat the
    // deadline; source/mirror pages have already been closed. A late browser
    // launch is closed by the cancellation guard immediately after launching.
    void pending.catch(() => {});
  }
}
