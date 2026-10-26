'use strict';

const fs = require('fs');
const path = require('path');

// tsc does not remove outputs for deleted/renamed sources. Derive coverage inputs
// from the source tree so switching branches cannot add obsolete dist modules.
function compiledSources(directory) {
  return fs.readdirSync(path.join(__dirname, directory), { withFileTypes: true }).flatMap(entry => {
    const name = `${directory}/${entry.name}`;
    if (entry.isDirectory()) return compiledSources(name);
    return name.endsWith('.ts') && !name.endsWith('.d.ts')
      ? [`dist/${name.replace(/\.ts$/, '.js')}`] : [];
  });
}

module.exports = {
  testEnvironment: 'node',
  testMatch: ['<rootDir>/test/**/*.test.js'],
  transformIgnorePatterns: ['/node_modules/(?!(pixelmatch)/)'],
  transform: {
    '[/\\\\]node_modules[/\\\\]pixelmatch[/\\\\].*\\.js$': 'babel-jest',
  },
  coverageProvider: 'v8',
  collectCoverageFrom: [
    ...compiledSources('lib'),
    'bench/report-utils.js',
    'eval/compare.js',
    // Browser snapshot integration remains in the separate Chromium suite.
    '!dist/lib/browser-snapshot.js',
    '!dist/lib/page-loader.js',
    '!dist/lib/snapshot-runner.js',
  ],
  coverageDirectory: '<rootDir>/coverage',
  coverageReporters: ['text', 'text-summary', 'lcov'],
  coverageThreshold: {
    global: { statements: 97, branches: 90, functions: 98, lines: 98 },
  },
};
