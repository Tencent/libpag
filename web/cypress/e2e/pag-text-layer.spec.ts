/* eslint-disable @typescript-eslint/no-invalid-this */
/* eslint-disable max-nested-callbacks  */
/* global describe it expect cy Cypress beforeEach afterEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGTextLayer } from '../../src/pag-text-layer';
import { visitTestPage } from './lane';

describe('PAGTextLayer', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let pagTextLayer: PAGTextLayer;
  let resources: { destroy: () => void }[] = [];

  beforeEach(() => {
    resources = [];
    visitTestPage();
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;

      const buffer = await global.fetch('/demo/assets/test2.pag').then((res) => res.arrayBuffer());
      const pagFile = await PAG.PAGFile.load(buffer);
      resources.push(pagFile);
      expect(pagFile.wasmIns).to.be.a('Object');
      const layers = pagFile.getLayersByEditableIndex(0, PAGTypes.LayerType.Text);
      resources.push(...layers);
      pagTextLayer = layers.get(0) as PAGTextLayer;
      layers.delete();
    });
  });

  afterEach(() => {
    for (const resource of resources.reverse()) {
      resource.destroy();
    }
    resources = [];
  });

  it('Get/Set font', () => {
    expect(pagTextLayer.wasmIns).to.be.a('Object');
    const originalFont = pagTextLayer.font();
    resources.push(originalFont);
    expect(originalFont.fontFamily).to.be.eq('Yuanti SC');
    const replacementFont = PAG.PAGFont.create('Arial', 'Regular');
    resources.push(replacementFont);
    pagTextLayer.setFont(replacementFont);
    const updatedFont = pagTextLayer.font();
    resources.push(updatedFont);
    expect(updatedFont.fontFamily).to.be.eq('Arial');
    expect(updatedFont.fontStyle).to.be.eq('Regular');
  });

  it('Get/Set fillColor', () => {
    const fillColor = pagTextLayer.fillColor();
    expect(fillColor).to.be.eql({ blue: 86, green: 35, red: 231 });
    fillColor.red = 255;
    pagTextLayer.setFillColor(fillColor);
    expect(pagTextLayer.fillColor().red).to.be.eq(255);
  });

  it('Get/Set fontSize', () => {
    expect(pagTextLayer.fontSize()).to.be.eq(120);
    pagTextLayer.setFontSize(125);
    expect(pagTextLayer.fontSize()).to.be.eq(125);
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
    expect(pagTextLayer.strokeColor()).to.be.eql({ red: 255, green: 235, blue: 235 });
  });

  it('Get/Set text', () => {
    expect(pagTextLayer.text()).to.be.eq('过去的一年里最开心的瞬间');
    pagTextLayer.setText('test');
    expect(pagTextLayer.text()).to.be.eq('test');
  });

  it('reset', () => {
    const originalFont = pagTextLayer.font();
    resources.push(originalFont);
    const replacementFont = PAG.PAGFont.create('Arial', 'Regular');
    resources.push(replacementFont);
    pagTextLayer.setFont(replacementFont);
    pagTextLayer.setFontSize(125);
    pagTextLayer.setFillColor({ red: 0, green: 0, blue: 0 });
    pagTextLayer.setStrokeColor({ red: 255, green: 0, blue: 0 });
    pagTextLayer.setText('changed');
    const changedFont = pagTextLayer.font();
    resources.push(changedFont);
    expect(changedFont.fontFamily).to.be.eq('Arial');
    expect(pagTextLayer.fontSize()).to.be.eq(125);
    expect(pagTextLayer.fillColor()).to.be.eql({ red: 0, green: 0, blue: 0 });
    expect(pagTextLayer.strokeColor()).to.be.eql({ red: 255, green: 0, blue: 0 });
    expect(pagTextLayer.text()).to.be.eq('changed');

    pagTextLayer.reset();
    const restoredFont = pagTextLayer.font();
    resources.push(restoredFont);
    expect(restoredFont.fontFamily).to.be.eq(originalFont.fontFamily);
    expect(restoredFont.fontStyle).to.be.eq(originalFont.fontStyle);
    expect(pagTextLayer.fontSize()).to.be.eq(120);
    expect(pagTextLayer.fillColor()).to.be.eql({ red: 231, green: 35, blue: 86 });
    expect(pagTextLayer.strokeColor()).to.be.eql({ red: 235, green: 235, blue: 235 });
    expect(pagTextLayer.text()).to.be.eq('过去的一年里最开心的瞬间');
  });
});
