/* eslint-disable @typescript-eslint/no-invalid-this */
/* eslint-disable max-nested-callbacks */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG, Rect } from '../../src/types';
import type { ScalerContext } from '../../src/interfaces';
import { visitTestPage } from './lane';

const fontSize = 16;
// Reference values captured on macOS with Arial. Only fontString and the render rect are
// asserted exactly; advances, bounds, and vertical metrics vary with the platform fallback
// font and are verified through invariants instead.
const scalerContextBaseline = {
  fontString:
    'italic bold 16px "Arial normal","Arial-normal","Arial",emoji,Arial,"Courier New",Georgia,"Times New Roman","Trebuchet MS",Verdana',
  bounds: { top: -12, bottom: 1, left: 0, right: 26 },
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
    // Advances depend on the platform fallback font, so assert the magnitude range and the
    // relative ordering instead of exact macOS Arial values.
    expect(scalerContext.getAdvance('text')).to.be.greaterThan(fontSize);
    expect(scalerContext.getAdvance('text')).to.be.lessThan(fontSize * 6);
    expect(scalerContext.getAdvance('t')).to.be.greaterThan(0);
    expect(scalerContext.getAdvance('text')).to.be.greaterThan(scalerContext.getAdvance('t'));
  });

  it('ScalerContext getBounds', () => {
    // Same reasoning as getAdvance: verify the geometry invariants and determinism.
    const bounds = scalerContext.getBounds('text', false, false) as Rect;
    expect(Number.isFinite(bounds.left + bounds.top + bounds.right + bounds.bottom)).to.equal(true);
    expect(bounds.left).to.be.lessThan(1);
    expect(bounds.top).to.be.lessThan(0);
    expect(bounds.right).to.be.greaterThan(bounds.left + fontSize);
    expect(bounds.bottom).to.be.greaterThan(bounds.top + fontSize * 0.5);
    expect(scalerContext.getBounds('text', false, false)).to.be.eql(bounds);
  });

  it('ScalerContext getFontMetrics', () => {
    // Vertical metrics vary with the platform fallback font; this SDK reports ascent above the
    // baseline as negative and descent below it as positive, so assert those relations and the
    // magnitudes that any sane font must satisfy.
    const metrics = scalerContext.getFontMetrics();
    expect(metrics.ascent).to.be.lessThan(0);
    expect(metrics.ascent).to.be.greaterThan(-fontSize * 1.5);
    expect(metrics.descent).to.be.greaterThan(0);
    expect(metrics.descent).to.be.lessThan(fontSize * 0.6);
    expect(metrics.xHeight).to.be.greaterThan(0);
    expect(metrics.xHeight).to.be.lessThan(-metrics.ascent);
    expect(metrics.capHeight).to.be.greaterThan(metrics.xHeight);
    expect(metrics.capHeight).to.be.lessThan(-metrics.ascent + 0.001);
    expect(scalerContext.getFontMetrics()).to.be.eql(metrics);
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
