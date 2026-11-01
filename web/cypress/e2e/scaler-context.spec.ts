/* eslint-disable @typescript-eslint/no-invalid-this */
/* eslint-disable max-nested-callbacks */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG, Rect } from '../../src/types';
import type { ScalerContext } from '../../src/interfaces';
import { visitTestPage } from './lane';

const scalerContextBaseline = {
  fontString:
    'italic bold 16px "Arial normal","Arial-normal","Arial",emoji,Arial,"Courier New",Georgia,"Times New Roman","Trebuchet MS",Verdana',
  bounds: { top: -12, bottom: 1, left: 0, right: 26 },
  advance: 25.7890625,
  fontMetrics: [-14, 3, 8.296875, 11.453125],
  bytesPerPixel: 4,
};

describe('PAGTextLayer', async () => {
  let PAG: PAG;
  let scalerContext: ScalerContext;

  beforeEach(() => {
    visitTestPage();
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      PAG = await window.libpag.PAGInit();
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
    expect(scalerContext.fontString(true, true)).to.be.eq(scalerContextBaseline.fontString);
  });

  it('ScalerContext getAdvance', () => {
    expect(scalerContext.getAdvance('text')).to.be.closeTo(scalerContextBaseline.advance, 0.001);
    expect(scalerContext.getAdvance('t')).to.be.greaterThan(0);
    expect(scalerContext.getAdvance('text')).to.be.greaterThan(scalerContext.getAdvance('t'));
  });

  it('ScalerContext getBounds', () => {
    const bounds = scalerContext.getBounds('text', false, false) as Rect;
    expect(bounds).to.be.eql(scalerContextBaseline.bounds);
  });

  it('ScalerContext getFontMetrics', () => {
    const metrics = scalerContext.getFontMetrics();
    expect(metrics.ascent).to.be.closeTo(scalerContextBaseline.fontMetrics[0], 0.001);
    expect(metrics.descent).to.be.closeTo(scalerContextBaseline.fontMetrics[1], 0.001);
    expect(metrics.xHeight).to.be.closeTo(scalerContextBaseline.fontMetrics[2], 0.001);
    expect(metrics.capHeight).to.be.closeTo(scalerContextBaseline.fontMetrics[3], 0.001);
    expect(metrics.capHeight).to.be.greaterThan(metrics.xHeight);
  });

  it('ScalerContext generateImage', () => {
    const image = scalerContext.readPixels('text', scalerContextBaseline.bounds, false);
    expect(image).to.be.a('Uint8Array');
    const width = scalerContextBaseline.bounds.right - scalerContextBaseline.bounds.left;
    const height = scalerContextBaseline.bounds.bottom - scalerContextBaseline.bounds.top;
    expect(image!.length).to.equal(width * height * scalerContextBaseline.bytesPerPixel);
    expect(Array.from(image!).some((alpha) => alpha > 0)).to.equal(true);
  });
});
