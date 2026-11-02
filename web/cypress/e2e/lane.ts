/**
 * Test-lane helpers for running the same spec files against the WebGL and WebGPU builds.
 * The lane is selected by the BACKEND cypress env: `cypress run --env BACKEND=webgpu`
 * switches every spec to the lib-webgpu build and enables the WebGPU-only tests.
 */
const backend = Cypress.env('BACKEND') ?? 'webgl';
if (backend !== 'webgl' && backend !== 'webgpu') {
  throw new Error(`Unsupported Cypress backend: ${backend}`);
}
export const isWebGPU = backend === 'webgpu';

/**
 * Registers a test that only runs on the WebGL lane (skipped when BACKEND=webgpu).
 */
export const itGL = isWebGPU ? it.skip : it;

/**
 * Registers a test that only runs on the WebGPU lane (skipped on the default WebGL lane).
 */
export const itWebGPU = isWebGPU ? it : it.skip;

interface AdapterWithInfo {
  info?: { isFallbackAdapter?: boolean };
  requestAdapterInfo?: () => Promise<{ isFallbackAdapter?: boolean }>;
}

interface NavigatorWithGPU {
  gpu?: { requestAdapter: () => Promise<AdapterWithInfo | null> };
}

let softwareWebGPUAdapter: boolean | undefined;

/**
 * Resolves whether the WebGPU lane runs on a software adapter such as SwiftShader. Always
 * false on the WebGL lane. The adapter type is browser-level, so it is cached across tests.
 */
export const isSoftwareWebGPU = async (): Promise<boolean> => {
  if (!isWebGPU) return false;
  if (softwareWebGPUAdapter === undefined) {
    // Cypress chainables are thenable at runtime but do not satisfy the Promise type.
    softwareWebGPUAdapter = await (cy.window().then(async (win) => {
      const gpu = (win.navigator as NavigatorWithGPU).gpu;
      const adapter = await gpu?.requestAdapter();
      const info = adapter?.info ?? (await adapter?.requestAdapterInfo?.());
      return info?.isFallbackAdapter === true;
    }) as unknown as Promise<boolean>);
  }
  return softwareWebGPUAdapter;
};

/**
 * Skips the current test on software WebGPU adapters. Reading canvas content back
 * (createImageBitmap on a WebGPU canvas, canvas2D compositing) is unavailable there and can
 * even crash the Dawn instance, so such tests are limited to hardware adapters.
 */
export const skipOnSoftwareWebGPU = async (context: Mocha.Context): Promise<void> => {
  if (await isSoftwareWebGPU()) {
    cy.log('skipped: requires a hardware WebGPU adapter');
    context.skip();
  }
};

/**
 * Visits the host page that loads the libpag build of the current lane.
 */
export const visitTestPage = () =>
  cy.visit(isWebGPU ? '/cypress/index-webgpu.html' : '/cypress/index.html');
