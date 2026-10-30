import { defineConfig } from 'cypress';
import * as fs from 'fs';
import * as path from 'path';
import * as brotli from 'brotli';

const baselinesDir = 'cypress/baselines/';

export default defineConfig({
  e2e: {
    baseUrl: 'http://127.0.0.1:8080/',
    specPattern: 'cypress/e2e/**/*.spec.ts',
    supportFile: 'cypress/support/e2e.js',
    defaultCommandTimeout: 20000,
    pageLoadTimeout: 60000,
    video: false,
    screenshotOnRunFailure: true,
    viewportWidth: 1280,
    viewportHeight: 720,
    retries: {
      runMode: 1,
      openMode: 0,
    },
    setupNodeEvents(on, config) {
      const backend = config.env.BACKEND ?? 'webgl';
      if (backend !== 'webgl' && backend !== 'webgpu') {
        throw new Error(`Unsupported Cypress backend: ${backend}`);
      }
      config.env.BACKEND = backend;
      const resultsDir = path.join('cypress/results', backend);
      fs.mkdirSync(resultsDir, { recursive: true });
      config.screenshotsFolder = path.join('cypress/screenshots', backend);
      config.reporter = 'junit';
      config.reporterOptions = { mochaFile: path.join(resultsDir, 'junit-[hash].xml'), toConsole: false };
      on('after:run', (results) => {
        fs.writeFileSync(path.join(resultsDir, 'summary.json'), JSON.stringify({ backend, ...results }, null, 2));
        if (results.status !== 'finished' || results.totalTests === 0 || results.totalPassed === 0) {
          throw new Error(`Cypress ${backend} did not complete any successful tests`);
        }
      });

      on('task', {
        readData(filename: string) {
          const filePath = path.join(baselinesDir, filename);
          if (fs.existsSync(filePath)) {
            const buffer = brotli.decompress(fs.readFileSync(filePath));
            return buffer;
          }
          return null;
        },
        saveData({ filename, data }: { filename: string; data: number[] }) {
          const buffer = Buffer.from(data);
          const output = brotli.compress(buffer);
          fs.writeFileSync(path.join(baselinesDir, filename), Buffer.from(output.buffer));
          return null;
        },
      });

      on('before:browser:launch', (browser, launchOptions) => {
        if (backend !== 'webgpu') return launchOptions;
        if (browser.name === 'electron' || browser.family !== 'chromium') {
          throw new Error('The WebGPU lane requires Chrome, Chromium or Edge with WebGPU enabled');
        }
        // The bundled Electron browser silently ignores launchOptions.args (it is launched
        // in-process via BrowserWindow, not as a separate browser process), so the WebGPU lane
        // must run on Chrome/Chromium/Edge for these flags to take effect.
        launchOptions.args.push('--enable-unsafe-webgpu');
        if (process.platform === 'linux') {
          // CI runners have no GPU hardware; fall back to the SwiftShader WebGPU adapter.
          launchOptions.args.push('--use-webgpu-adapter=swiftshader');
        }
        return launchOptions;
      });
      return config;
    },
  },
});
