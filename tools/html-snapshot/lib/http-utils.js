// HTTP-layer helpers shared by server.js. Kept deliberately small: only the
// pieces that several request handlers reach for live here. Pipeline-specific
// logic (readBody's drain semantics, json envelope shaping, etc.) stays in
// server.js so this file remains a generic toolkit.

'use strict';

// Tagged error so request handlers can map a single throw into the right
// HTTP status + JSON body without losing the cause chain. Helpers in this
// module throw `HttpError` for client-facing 4xx contract violations; the
// server's error mapper turns the throw into a clean response.
class HttpError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
}

// Best-effort extraction of a single query parameter from `req.url`. Returns
// `null` when the URL is malformed (e.g. unparseable percent-encoding) so
// the caller can treat junk URLs as "no parameter given" instead of having
// the whole request fail at the routing layer — the body parser, when it
// runs next, will surface the real problem.
function safeQueryParam(req, key) {
  try {
    return new URL(req.url, 'http://localhost').searchParams.get(key);
  } catch (_) {
    return null;
  }
}

// Coerce the named query parameter to a number and assign it to `target[key]`
// if the parameter was present. NaN is intentionally preserved on bad input
// so the downstream option resolver can drop it and fall back to a default —
// matching the existing "lenient query, strict body" contract.
function setNumberParam(params, target, key) {
  const raw = params.get(key);
  if (raw !== null) target[key] = Number(raw);
}

// Assign a string query parameter through to `target[key]` when present.
// Trivial wrapper, but worth the symmetry with the number/bool variants so
// the call sites in `parseGetSource` read uniformly.
function setStringParam(params, target, key) {
  const raw = params.get(key);
  if (raw !== null) target[key] = raw;
}

// Parse a query parameter as a boolean (`true|false|1|0`) and assign it to
// `target[key]`. Any other non-empty value triggers `HttpError 400` — silent
// fallback would leave the caller wondering why their toggle didn't take
// effect, which is a worse failure mode than a clear 400.
function setBoolParam(params, target, key) {
  const raw = params.get(key);
  if (raw === null) return;
  if (raw === 'true' || raw === '1') target[key] = true;
  else if (raw === 'false' || raw === '0') target[key] = false;
  else throw new HttpError(400, `${key} must be true|false|1|0, got '${raw}'`);
}

// Decide whether the response body should be JSON based on the Accept
// header. Conservative on purpose: only return true when JSON is explicitly
// requested AND text/html is NOT also listed. Browsers send the catch-all
// `text/html, application/xhtml+xml, …, */*` and should still get HTML
// responses when they hit /snapshot directly.
function wantsJsonResponse(req) {
  const accept = (req.headers['accept'] || '').toLowerCase();
  if (!accept) return false;
  if (!accept.includes('application/json')) return false;
  if (accept.includes('text/html')) return false;
  return true;
}

module.exports = {
  HttpError,
  safeQueryParam,
  setNumberParam,
  setStringParam,
  setBoolParam,
  wantsJsonResponse,
};
