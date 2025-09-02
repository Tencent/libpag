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
    cy.visit('/cypress/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
      const buffer = await global.fetch('/demo/assets/AudioMarker.pag').then((res) => res.arrayBuffer());
      const pagFile = await PAG.PAGFile.load(buffer);
      pagImageLayer = pagFile.getLayersByEditableIndex(0, PAGTypes.LayerType.Image).get(0) as PAGImageLayer;
    });
  });

  let pagImageLayer: PAGImageLayer;

  it('Get contentDuration', async () => {
    expect(pagImageLayer.contentDuration()).to.be.eq(19440000);
  });

  it('Get videoRanges', async () => {
    expect(pagImageLayer.getVideoRanges().length).to.be.eq(5);
  });

  it('Replace Image', async () => {
    const imageBlob = await global.fetch('/demo/assets/cat.png').then((res) => res.blob());
    const pagImage = await PAG.PAGImage.fromFile(new File([imageBlob], 'cat.png'));
    expect(pagImage.wasmIns).to.be.a('object');
    pagImageLayer.replaceImage(pagImage);
  });

  it('Set Image', async () => {
    const imageBlob = await global.fetch('/demo/assets/cat.png').then((res) => res.blob());
    const pagImage = await PAG.PAGImage.fromFile(new File([imageBlob], 'cat.png'));
    expect(pagImage.wasmIns).to.be.a('object');
    pagImageLayer.setImage(pagImage);
  });

  it('layerTime to content', async () => {
    expect(pagImageLayer.layerTimeToContent(0)).to.be.eq(0);
  });

  it('contentTime to layer', async () => {
    expect(pagImageLayer.contentTimeToLayer(0)).to.be.eq(880000);
  });
});
