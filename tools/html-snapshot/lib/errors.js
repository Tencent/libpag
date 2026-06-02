// Shared base class for "a child process we spawned exited non-zero or
// failed to start". Both `PipelineStepError` (lib/pipeline.js) and
// `PagxImportError` (lib/pagx-runner.js) carry the same `{ code, stderr }`
// pair on top of an Error message; this base captures the pattern so
// future child-process wrappers do not have to retype the same constructor.
//
// Why a base class rather than a single concrete error: each call site
// wants its own `name` (`PipelineStepError` vs `PagxImportError`) so
// `instanceof` checks and stringified stack traces stay specific. Sharing a
// base only deduplicates the field initialisation; the named subclasses
// keep their identity, including any extra fields they need (e.g. the
// pipeline error's `step` label).
//
// Subclasses set `this.name` themselves so the constructor stays a single
// line at the call site and the base does not have to know which subclass
// instantiated it.

'use strict';

class ChildProcessError extends Error {
  constructor(message, { code, stderr } = {}) {
    super(message);
    this.name = 'ChildProcessError';
    this.code = code === undefined ? null : code;
    this.stderr = stderr || '';
  }
}

module.exports = { ChildProcessError };
