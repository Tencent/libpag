// Babel config used only by Jest (babel-jest). The source tree itself is
// compiled to CommonJS by TypeScript and never Babel-compiled at runtime.
// Jest only applies this transform to pixelmatch; leaving dist untouched preserves
// the TypeScript source maps used by V8 coverage and serialized browser callbacks.
// We need Babel solely so Jest can transform the handful of
// ESM-only dependencies (e.g. pixelmatch v7) that `require()` pulls in —
// see `transformIgnorePatterns` in package.json. Targeting the current Node
// keeps the transform minimal (CommonJS interop only, no syntax down-levelling).
'use strict';

module.exports = {
  presets: [
    ['@babel/preset-env', { targets: { node: 'current' } }],
  ],
};
