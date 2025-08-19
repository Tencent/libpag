module.exports = {
  extends: ['alloy', 'alloy/typescript'],
  env: {
    browser: true,
    worker: true,
  },
  rules: {
    'max-params': ['error', 9],
  },
  parserOptions: {
    lib: ['dom', 'ES5', 'ES6', 'DOM.Iterable'],
  },
  globals: {
    globalThis: true,
    DedicatedWorkerGlobalScope: false,
  },
};
