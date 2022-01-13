module.exports = (api) => {
  const isTest = api.env('test');
  const testBabel = {
    presets: [['@babel/preset-env', { targets: { node: 'current' } }]],
  };
  const prodBabel = {
    presets: [
      [
        '@babel/preset-env',
        {
          modules: false,
          useBuiltIns: 'usage',
          corejs: 3,
          targets: {
            browsers: ['> 1%', 'last 2 versions', 'not ie <= 8'],
          },
        },
      ],
    ],
  };
  return isTest ? testBabel : prodBabel;
};
