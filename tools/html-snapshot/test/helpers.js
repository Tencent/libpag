// Shared test helpers. Several lib/* functions report fatal CLI errors by
// writing to console.error and calling process.exit(2) (see lib/cli.js's
// `fail`). To exercise those branches without tearing down the test runner,
// `captureExit` temporarily swaps process.exit for a throwing stub and
// silences console.error, then restores both — returning the captured exit
// code and message so assertions can inspect them.

'use strict';

// Run `fn`, intercepting any process.exit() call. Returns
// `{ exited, code, messages }`:
//   exited   — true when fn triggered process.exit
//   code     — the exit code passed to process.exit (undefined if not exited)
//   messages — every string written to console.error during the call
// The original process.exit / console.error are always restored, even when
// `fn` throws for an unrelated reason.
function captureExit(fn) {
  const realExit = process.exit;
  const realError = console.error;
  const messages = [];
  let exited = false;
  let code;
  // Sentinel thrown by the exit stub so control unwinds out of `fn` exactly
  // the way a real process.exit would end execution, without killing the
  // test process.
  const EXIT = Symbol('exit');
  process.exit = (c) => {
    exited = true;
    code = c;
    throw EXIT;
  };
  console.error = (msg) => { messages.push(String(msg)); };
  try {
    fn();
  } catch (err) {
    if (err !== EXIT) {
      console.error = realError;
      process.exit = realExit;
      throw err;
    }
  } finally {
    console.error = realError;
    process.exit = realExit;
  }
  return { exited, code, messages };
}

// Build a fake argv that mirrors process.argv shape: lib/cli.js's parseArgs
// slices past argv[0] (node) and argv[1] (script), so tests pass only the
// user-facing tokens here.
function argv(...args) {
  return ['node', 'snapshot.js', ...args];
}

module.exports = { captureExit, argv };
