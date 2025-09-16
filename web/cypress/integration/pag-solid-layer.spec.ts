/* eslint-disable @typescript-eslint/no-invalid-this */
/* eslint-disable max-nested-callbacks  */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import { Color, PAG } from '../../src/types';
import type { PAGSolidLayer } from '../../src/pag-solid-layer';
import type { PAGFile } from '../../src/pag-file';
import type { PAGLayer } from '../../src/pag-layer';

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

  let pagFile: PAGFile;
  let pagLayer: PAGSolidLayer;

  it('Get PAGSolidLayer', async () => {
    const buffer = await global.fetch('/demo/assets/test2.pag').then((res) => res.arrayBuffer());
    pagFile = await PAG.PAGFile.load(buffer);
    pagLayer = pagFile.getLayerAt(0) as PAGSolidLayer;
    expect(pagLayer.uniqueID()).to.be.eq(16);
  });

  it('Get/Set solidColor', () => {
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
