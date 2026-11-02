/* global describe it expect cy */
import type * as Libpag from '../../src/pag';

describe('SDK initialization', () => {
  it('loads the UMD SDK and resolves wasm relative to its script', () => {
    cy.intercept('GET', '**/libpag.wasm').as('wasm');
    cy.visit('/cypress/index.html');
    cy.window().its('libpag.PAGInit').should('be.a', 'function');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      const module = await window.libpag.PAGInit();
      expect(module.Matrix).to.be.a('function');
    });
    cy.wait('@wasm').then(({ request, response }) => {
      expect(request.url).to.match(/\/lib\/libpag\.wasm$/);
      expect(response?.statusCode).to.equal(200);
    });
  });
});
