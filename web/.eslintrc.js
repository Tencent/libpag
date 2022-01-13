module.exports = {
  extends: ['alloy', 'alloy/typescript'],
  env: {
    browser: true,
  },
  rules: {
    'max-params': ['error', 8],
  },
  parserOptions: {
    lib: ['dom', 'ES5', 'ES6', 'DOM.Iterable'],
  },
};
