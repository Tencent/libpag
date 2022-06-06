/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress */
import type * as Libpag from '../../src/pag';

describe('PAG Load', () => {
  it('PAGInit', () => {
    cy.visit('/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      const PAG = await window.libpag.PAGInit();
      expect(PAG).to.be.a('object');
    });
  });
});
