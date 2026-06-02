'use strict';

const { ChildProcessError } = require('../dist/lib/errors');

describe('ChildProcessError', () => {
  test('extends Error and carries the message', () => {
    const err = new ChildProcessError('boom');
    expect(err).toBeInstanceOf(Error);
    expect(err).toBeInstanceOf(ChildProcessError);
    expect(err.message).toBe('boom');
    expect(err.name).toBe('ChildProcessError');
  });

  test('defaults code to null and stderr to empty string', () => {
    const err = new ChildProcessError('boom');
    expect(err.code).toBeNull();
    expect(err.stderr).toBe('');
  });

  test('honours an explicit { code, stderr } payload', () => {
    const err = new ChildProcessError('failed', { code: 137, stderr: 'oom\n' });
    expect(err.code).toBe(137);
    expect(err.stderr).toBe('oom\n');
  });

  test('treats an explicit zero exit code as zero, not null', () => {
    // The pipeline reports {code: 0} for a "tool ran clean but produced no
    // output" failure; the field must round-trip exactly so callers can
    // distinguish that from a genuinely missing code.
    const err = new ChildProcessError('weird', { code: 0 });
    expect(err.code).toBe(0);
  });

  test('subclasses keep their own name and pick up the base fields', () => {
    class PipelineStepError extends ChildProcessError {
      constructor(message, opts = {}) {
        super(message, opts);
        this.name = 'PipelineStepError';
        this.step = opts.step || '';
      }
    }
    const err = new PipelineStepError('snapshot failed', {
      code: 1,
      stderr: 'oops',
      step: 'snapshot',
    });
    expect(err).toBeInstanceOf(ChildProcessError);
    expect(err.name).toBe('PipelineStepError');
    expect(err.code).toBe(1);
    expect(err.stderr).toBe('oops');
    expect(err.step).toBe('snapshot');
  });

  test('stack trace points back to the throw site', () => {
    function thrower() { throw new ChildProcessError('x'); }
    expect(() => thrower()).toThrow(ChildProcessError);
    let captured;
    try { thrower(); } catch (err) { captured = err; }
    expect(captured.stack).toContain('thrower');
  });
});
