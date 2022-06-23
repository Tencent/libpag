/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGImageLayer } from '../../src/pag-image-layer';

describe('PAGImage', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let layers: Libpag.types.Vector<PAGImageLayer>;
  beforeEach(() => {
    cy.visit('/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;

      const buffer = await global
        .fetch('http://127.0.0.1:8080/demo/assets/AudioMarker.pag')
        .then((res) => res.arrayBuffer());
      const pagFile = await PAG.PAGFile.load(buffer);
      layers = pagFile.getLayersByEditableIndex(0, PAGTypes.LayerType.Image) as Libpag.types.Vector<PAGImageLayer>;
    });
  });

  it('size', () => {
    expect(layers.size()).to.be.eq(1);
  });

  it('Get', () => {
    expect(layers.get(0).wasmIns).to.be.a('Object');
  });

  it('Push back', () => {
    const layer = layers.get(0);
    layers.push_back(layer);
    expect(layers.size()).to.be.eq(2);
  });
});
