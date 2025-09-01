/* eslint-disable @typescript-eslint/no-invalid-this */
/* eslint-disable max-nested-callbacks  */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';

describe('PAGTextLayer', async () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;

  beforeEach(() => {
    cy.visit('/cypress/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
    });
  });

  it('test emoji', async () => {
    const { ScalerContext } = PAG;
    expect(ScalerContext.isEmoji('测试')).to.be.eq(false);
    expect(ScalerContext.isEmoji('👍')).to.be.eq(true);
    expect(ScalerContext.isEmoji('👪')).to.be.eq(true);
    expect(ScalerContext.isEmoji('🇨🇳')).to.be.eq(true);
    expect(ScalerContext.isEmoji('1⃣️')).to.be.eq(true);
  });
});
