/* global describe it expect cy */
import type * as Libpag from '../../src/pag';
import { isWebGPU, visitTestPage } from './lane';

describe('SDK initialization', () => {
  it('loads the UMD SDK and resolves wasm relative to its script', () => {
    visitTestPage();
    cy.window().its('libpag.PAGInit').should('be.a', 'function');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      window.performance.clearResourceTimings();
      const module = await window.libpag.PAGInit();
      expect(module.Matrix).to.be.a('function');
      expect(module._IsWebGPUBackend(), 'actual WASM backend').to.equal(isWebGPU);
      // Intercepting the binary copies large debug WASM responses into Cypress's control channel.
      const wasmPath = isWebGPU ? '/lib-webgpu/libpag.wasm' : '/lib/libpag.wasm';
      const wasmURL = new URL(wasmPath, window.location.href).href;
      const loads = window.performance.getEntriesByName(wasmURL, 'resource');
      expect(loads.length, 'default WASM URL loaded during successful initialization').to.be.greaterThan(0);
      if (isWebGPU) {
        const device = module.preinitializedWebGPUDevice;
        if (!device || !('destroy' in device) || typeof device.destroy !== 'function') {
          throw new Error('The initialized WebGPU device must support explicit destruction');
        }
        device.destroy();
      }
    });
  });
});
