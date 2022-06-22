module.exports = {
  extends: ['alloy', 'alloy/typescript'],
  env: {
    browser: true,
  },
  rules: {
    'max-params': ['error', 9],
  },
  parserOptions: {
    lib: ['dom', 'ES5', 'ES6', 'DOM.Iterable'],
  },
};
