/* eslint-disable @typescript-eslint/no-invalid-this */
/* eslint-disable max-nested-callbacks  */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGImageLayer } from '../../src/pag-image-layer';

describe('PAGImageLayer', async () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  beforeEach(() => {
    cy.visit('/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
    });
  });

  let pagImageLayer: PAGImageLayer;

  it('Get contentDuration', async () => {
    const buffer = await global
      .fetch('http://127.0.0.1:8080/demo/assets/AudioMarker.pag')
      .then((res) => res.arrayBuffer());
    const pagFile = await PAG.PAGFile.load(buffer);
    pagImageLayer = pagFile.getLayersByEditableIndex(0, PAGTypes.LayerType.Image).get(0) as PAGImageLayer;
    expect(pagImageLayer.contentDuration()).to.be.eq(19440000);
  });

  it('Get videoRanges', async () => {
    expect(pagImageLayer.getVideoRanges().size()).to.be.eq(5);
  });

  it('layerTime to content', async () => {
    expect(pagImageLayer.layerTimeToContent(0)).to.be.eq(0);
  });

  it('contentTime to layer', async () => {
    expect(pagImageLayer.contentTimeToLayer(0)).to.be.eq(880000);
  });
});
