// Shared `page.on('response')` listener factory used by lib/snapshot-runner.ts
// (image capture) and lib/font-download.ts (font capture).
//
// Both call sites had near-identical code:
//   - try/catch around resp.request() / url / cache lookup;
//   - filter by resourceType() and skip data: / non-OK responses;
//   - read the body bytes (eagerly — the response object may not survive
//     past navigation/teardown);
//   - store a derived value into a `url -> value` Map keyed by URL;
//   - swallow errors with a one-line "<label> capture skipped: ..." log.
//
// The font path additionally races the body read against a hard timeout (a
// hung CDN must not stall the snapshot); the image path stores `{ buffer,
// contentType }` instead of a bare Buffer. Both are captured here as
// parameters: `timeoutMs` is optional, and `project(buf, resp)` builds the
// stored value (defaults to `buf`).

import { responseBytes, type BrowserResponse, type EngineName } from './browser-engine';
import { errMessage } from './common';

export type CaptureLogger = (msg: string) => void;

export interface CaptureListenerOptions<TValue> {
  resourceType: string;
  engine: EngineName;
  cache: Map<string, TValue>;
  log?: CaptureLogger | null;
  label?: string;
  project?: (buf: Buffer, resp: BrowserResponse) => TValue;
  timeoutMs?: number;
  timeoutMessage?: string;
  // When true, an empty body is dropped (no cache.set), so the URL re-enters
  // the cache miss path on next access. The font path uses this; the image
  // path leaves it false to preserve its long-standing "cache the empty
  // entry" behaviour (downstream tolerates empty buffers and the in-page
  // fetch fallback is more expensive than a no-op data: URI).
  skipEmptyBody?: boolean;
}

export type CaptureResponseListener = (resp: BrowserResponse) => Promise<void>;

export function makeCaptureListener<TValue>({
  resourceType,
  engine,
  cache,
  log = null,
  label,
  project,
  timeoutMs = 0,
  timeoutMessage,
  skipEmptyBody = false,
}: CaptureListenerOptions<TValue>): CaptureResponseListener {
  const resolvedLabel = label || resourceType;
  // Identity projection: only safe when TValue is Buffer; the type system
  // can't enforce that here without making `project` always required, so we
  // assert via cast — both call sites that omit `project` use Buffer.
  const projectFn: (buf: Buffer, resp: BrowserResponse) => TValue =
    project || ((buf: Buffer): TValue => buf as unknown as TValue);
  return async function captureResponse(resp: BrowserResponse): Promise<void> {
    try {
      const req = resp.request();
      if (req.resourceType() !== resourceType) return;
      const url = req.url();
      if (!url || url.startsWith('data:')) return;
      if (!resp.ok()) return;
      if (cache.has(url)) return;
      let buf: Buffer;
      if (timeoutMs > 0) {
        // Clear the timer once the race settles (either branch) so a
        // successfully-read body never leaves the timer pending — an
        // uncleared timer keeps the Node event loop alive past the work it
        // was guarding.
        let timer: NodeJS.Timeout | undefined;
        buf = await Promise.race([
          responseBytes(resp, engine),
          new Promise<Buffer>((_resolve, reject) => {
            timer = setTimeout(
              () => reject(new Error(timeoutMessage || `timed out reading ${resolvedLabel} body`)),
              timeoutMs,
            );
          }),
        ]).finally(() => clearTimeout(timer));
      } else {
        buf = await responseBytes(resp, engine);
      }
      if (skipEmptyBody && (!buf || !buf.length)) return;
      cache.set(url, projectFn(buf, resp));
    } catch (err) {
      // Common case: response detached after a navigation, or the body was
      // served from cache and is no longer accessible. Drop it — callers
      // either fall back to an in-page fetch (image) or accept the URL
      // absent (font).
      if (log) log(`${resolvedLabel} capture skipped: ${errMessage(err)}`);
    }
  };
}
