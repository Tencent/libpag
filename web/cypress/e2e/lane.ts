/**
 * Test-lane helpers for running the same spec files against the WebGL and WebGPU builds.
 * The lane is selected by the BACKEND cypress env: `cypress run --env BACKEND=webgpu`
 * switches every spec to the lib-webgpu build and enables the WebGPU-only tests.
 */
export const isWebGPU = Cypress.env('BACKEND') === 'webgpu';

/**
 * Registers a test that only runs on the WebGL lane (skipped when BACKEND=webgpu).
 */
export const itGL = isWebGPU ? it.skip : it;

/**
 * Registers a test that only runs on the WebGPU lane (skipped on the default WebGL lane).
 */
export const itWebGPU = isWebGPU ? it : it.skip;

/**
 * Visits the host page that loads the libpag build of the current lane.
 */
export const visitTestPage = () =>
  cy.visit(isWebGPU ? '/cypress/index-webgpu.html' : '/cypress/index.html');
