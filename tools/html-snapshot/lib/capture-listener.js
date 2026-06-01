// Shared `page.on('response')` listener factory used by lib/snapshot-runner.js
// (image capture) and lib/font-download.js (font capture).
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

'use strict';

const { responseBytes } = require('./browser-engine');
const { errMessage } = require('./cli');

function makeCaptureListener({
  resourceType,
  engine,
  cache,
  log,
  label = resourceType,
  project = (buf) => buf,
  timeoutMs = 0,
  timeoutMessage,
  // When true, an empty body is dropped (no cache.set), so the URL re-enters
  // the cache miss path on next access. The font path uses this; the image
  // path leaves it false to preserve its long-standing "cache the empty
  // entry" behaviour (downstream tolerates empty buffers and the in-page
  // fetch fallback is more expensive than a no-op data: URI).
  skipEmptyBody = false,
}) {
  return async function captureResponse(resp) {
    try {
      const req = resp.request();
      if (req.resourceType() !== resourceType) return;
      const url = req.url();
      if (!url || url.startsWith('data:')) return;
      if (!resp.ok()) return;
      if (cache.has(url)) return;
      let buf;
      if (timeoutMs > 0) {
        // Clear the timer once the race settles (either branch) so a
        // successfully-read body never leaves the timer pending — an
        // uncleared timer keeps the Node event loop alive past the work it
        // was guarding.
        let timer;
        buf = await Promise.race([
          responseBytes(resp, engine),
          new Promise((_, reject) => {
            timer = setTimeout(
              () => reject(new Error(timeoutMessage || `timed out reading ${label} body`)),
              timeoutMs,
            );
          }),
        ]).finally(() => clearTimeout(timer));
      } else {
        buf = await responseBytes(resp, engine);
      }
      if (skipEmptyBody && (!buf || !buf.length)) return;
      cache.set(url, project(buf, resp));
    } catch (err) {
      // Common case: response detached after a navigation, or the body was
      // served from cache and is no longer accessible. Drop it — callers
      // either fall back to an in-page fetch (image) or accept the URL
      // absent (font).
      if (log) log(`${label} capture skipped: ${errMessage(err)}`);
    }
  };
}

module.exports = { makeCaptureListener };
