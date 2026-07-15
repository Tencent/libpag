// Babel config used only by Jest (babel-jest). The source tree itself is
// plain CommonJS that runs under Node directly and is never Babel-compiled at
// runtime. We need Babel solely so Jest can transform the handful of
// ESM-only dependencies (e.g. pixelmatch v7) that `require()` pulls in —
// see `transformIgnorePatterns` in package.json. Targeting the current Node
// keeps the transform minimal (CommonJS interop only, no syntax down-levelling).
'use strict';

module.exports = {
  presets: [
    ['@babel/preset-env', { targets: { node: 'current' } }],
  ],
};
