import { defineConfig } from 'cypress';

const baselinesDir = 'cypress/baselines/';

export default defineConfig({
  e2e: {
    baseUrl: 'http://localhost:8080/',
    specPattern: 'cypress/e2e/**/*.spec.ts',
    supportFile: 'cypress/support/e2e.js',
    defaultCommandTimeout: 20000,
    pageLoadTimeout: 60000,
    video: false,
    screenshotOnRunFailure: false,
    viewportWidth: 1280,
    viewportHeight: 720,
    retries: {
      runMode: 1,
      openMode: 0,
    },
    setupNodeEvents(on, config) {
      const fs = require('fs');
      const path = require('path');
      const brotli = require('brotli');

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
        if (config.env.BACKEND !== 'webgpu' || browser.name === 'electron') {
          return launchOptions;
        }
        if (browser.family !== 'chromium') {
          return launchOptions;
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
    },
  },
});
