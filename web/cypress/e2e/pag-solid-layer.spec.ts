/* eslint prefer-arrow-callback: ["error", { "allowNamedFunctions": true }] */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* eslint-disable max-nested-callbacks  */
/* global describe it expect cy Cypress beforeEach afterEach */
import type * as Libpag from '../../src/pag';
import type { PAGSolidLayer } from '../../src/pag-solid-layer';
import type { PAGFile } from '../../src/pag-file';
import { visitTestPage } from './lane';

describe('PAGSolidLayer', function solidLayerTests() {
  let pagFile: PAGFile | undefined;
  let pagLayer: PAGSolidLayer | undefined;

  beforeEach(function setupSolidLayer() {
    pagFile = undefined;
    pagLayer = undefined;
    visitTestPage();
    cy.window().then(async function loadSolidLayer(window: Cypress.AUTWindow & { libpag: typeof Libpag }) {
      const PAG = await window.libpag.PAGInit();
      const response = await window.fetch('/demo/assets/test2.pag');
      pagFile = await PAG.PAGFile.load(await response.arrayBuffer());
      pagLayer = pagFile.getLayerAt(0) as PAGSolidLayer;
    });
  });

  afterEach(function destroySolidLayer() {
    pagLayer?.destroy();
    pagFile?.destroy();
    pagLayer = undefined;
    pagFile = undefined;
  });

  it('Get PAGSolidLayer', function getSolidLayer() {
    expect(pagLayer.wasmIns).to.be.a('object');
    expect(pagLayer.layerName()).to.be.eq('SolidLayer');
    expect(pagLayer.uniqueID()).to.be.greaterThan(0);
  });

  it('Get/Set solidColor', function setSolidColor() {
    let color = pagLayer.solidColor();
    expect([color.red, color.blue, color.green]).to.be.eql([236, 236, 236]);
    const newColor = {
      red: 244,
      blue: 245,
      green: 247,
    };
    pagLayer.setSolidColor(newColor);
    color = pagLayer.solidColor();
    expect([color.red, color.blue, color.green]).to.be.eql([244, 245, 247]);
  });
});
