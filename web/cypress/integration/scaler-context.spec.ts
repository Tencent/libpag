/* eslint-disable @typescript-eslint/no-invalid-this */
/* eslint-disable max-nested-callbacks  */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG, Rect } from '../../src/types';
import type { ScalerContext } from '../../src/interfaces';

const scalerContextBaseline = {
  fontString:
    'italic bold 16px "Arial normal","Arial-normal","Arial",emoji,Arial,"Courier New",Georgia,"Times New Roman","Trebuchet MS",Verdana',
  bounds: [-12, 1, 0, 26],
  fontMetrics: [-14, 3, 8.296875, 11.453125],
};

describe('PAGTextLayer', async () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let scalerContext: ScalerContext;

  beforeEach(() => {
    cy.visit('/cypress/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
      scalerContext = new PAG.ScalerContext('Arial', 'normal', 16, false, false);
    });
  });

  it('ScalerContext isEmoji', async () => {
    expect(PAG.ScalerContext.isEmoji('测试')).to.be.eq(false);
    expect(PAG.ScalerContext.isEmoji('👍')).to.be.eq(true);
    expect(PAG.ScalerContext.isEmoji('👪')).to.be.eq(true);
    expect(PAG.ScalerContext.isEmoji('🇨🇳')).to.be.eq(true);
    expect(PAG.ScalerContext.isEmoji('1⃣️')).to.be.eq(true);
  });

  it('ScalerContext fontString', () => {
    const fontString = scalerContext.fontString(true, true);
    expect(fontString).to.be.eq(scalerContextBaseline.fontString);
  });

  it('ScalerContext getAdvance', () => {
    expect(scalerContext.getAdvance('text')).to.be.a('number');
  });

  it('ScalerContext getBounds', () => {
    scalerContext.getBounds('text', false, false) as Rect;
  });

  it('ScalerContext getFontMetrics', () => {
    scalerContext.getFontMetrics();
  });

  it('ScalerContext generateImage', () => {
    const rect: Rect = {
      top: -12,
      bottom: 1,
      left: 0,
      right: 26,
    };
    const image = scalerContext.readPixels('text', rect, false);
    expect(image).to.be.a('Uint8Array');
  });
});
