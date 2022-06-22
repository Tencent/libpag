/* eslint-disable @typescript-eslint/no-invalid-this */
/* eslint-disable max-nested-callbacks  */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGTextLayer } from '../../src/pag-text-layer';

describe('PAGTextLayer', async () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let pagTextLayer: PAGTextLayer;

  beforeEach(() => {
    cy.visit('/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;

      const buffer = await global.fetch('http://127.0.0.1:8080/demo/assets/test2.pag').then((res) => res.arrayBuffer());
      const pagFile = await PAG.PAGFile.load(buffer);
      expect(pagFile.wasmIns).to.be.a('Object');
      const layers = pagFile.getLayersByEditableIndex(0, PAGTypes.LayerType.Text);
      pagTextLayer = layers.get(0) as PAGTextLayer;
    });
  });

  it('Get/Set font', async () => {
    expect(pagTextLayer.wasmIns).to.be.a('Object');
    expect(pagTextLayer.font().fontFamily).to.be.eq('Yuanti SC');
    pagTextLayer.setFont(PAG.PAGFont.create('Arial', 'Regular'));
    expect(pagTextLayer.font().fontFamily).to.be.eq('Arial');
  });

  it('Get/Set fillColor', async () => {
    const fillColor = pagTextLayer.fillColor();
    expect(fillColor).to.be.eql({ blue: 86, green: 35, red: 231 });
    fillColor.red = 255;
    pagTextLayer.setFillColor(fillColor);
    expect(pagTextLayer.fillColor().red).to.be.eq(255);
  });

  it('Get/Set strokeColor', () => {
    const strokeColor = pagTextLayer.strokeColor();
    expect(strokeColor).to.be.eql({
      red: 235,
      green: 235,
      blue: 235,
    });
    strokeColor.red = 255;
    pagTextLayer.setStrokeColor(strokeColor);
    expect(strokeColor.red).to.be.eq(255);
  });

  it('Get/Set text', () => {
    expect(pagTextLayer.text()).to.be.eq('过去的一年里最开心的瞬间');
    pagTextLayer.setText('test');
    expect(pagTextLayer.text()).to.be.eq('test');
  });

  it('reset', () => {
    pagTextLayer.reset();
    expect(pagTextLayer.strokeColor()).to.be.eql({
      red: 235,
      green: 235,
      blue: 235,
    });
    expect(pagTextLayer.text()).to.be.eq('过去的一年里最开心的瞬间');
  });
});
