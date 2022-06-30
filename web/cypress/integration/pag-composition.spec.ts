/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAGComposition } from '../../src/pag-composition';
import type { PAGLayer } from '../../src/pag-layer';
import type { PAG } from '../../src/types';

describe('PAGImage', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  beforeEach(() => {
    cy.visit('/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
    });
  });

  let pagComposition: PAGComposition;
  it('Make', async () => {
    pagComposition = PAG.PAGComposition.make(100, 100);
    expect(pagComposition.wasmIns).to.be.a('object');
  });

  it('Get size', async () => {
    const buffer = await global
      .fetch('http://127.0.0.1:8080/demo/assets/AudioMarker.pag')
      .then((res) => res.arrayBuffer());
    pagComposition = (await PAG.PAGFile.load(buffer)) as PAGComposition;
    expect(pagComposition.wasmIns).to.be.a('object');
    expect(pagComposition.width()).to.be.eq(720);
    expect(pagComposition.height()).to.be.eq(1280);
  });

  it('Set content size', async () => {
    pagComposition.setContentSize(1000, 1000);
    expect(pagComposition.width()).to.be.eq(1000);
    expect(pagComposition.height()).to.be.eq(1000);
  });

  let numChildren: number;
  it('Get children number', async () => {
    numChildren = pagComposition.numChildren();
    expect(pagComposition.numChildren()).to.be.eq(3);
  });

  let pagLayer: PAGLayer;
  it('Get layer at index', async () => {
    pagLayer = pagComposition.getLayerAt(0);
    expect(pagLayer.wasmIns).to.be.a('Object');
  });

  it('Get layer index', async () => {
    expect(pagComposition.getLayerIndex(pagLayer)).to.be.eq(0);
  });

  it('Set layer index', async () => {
    pagComposition.setLayerIndex(pagLayer, 1);
    expect(pagComposition.getLayerIndex(pagLayer)).to.be.eq(1);
  });

  let pagLayerDelete: PAGLayer | null;
  it('Remove layer', async () => {
    pagLayerDelete = pagComposition.removeLayer(pagLayer);
    pagLayerDelete.destroy();
    pagLayerDelete = null;
    expect(pagComposition.numChildren()).to.be.eq(numChildren - 1);
  });

  it('Add layer', async () => {
    pagComposition.addLayer(pagLayer);
    expect(pagComposition.numChildren()).to.be.eq(numChildren);
  });

  it('Remove layer at index', async () => {
    pagLayerDelete = pagComposition.removeLayerAt(2);
    pagLayerDelete.destroy();
    pagLayerDelete = null;
    expect(pagComposition.numChildren()).to.be.eq(numChildren - 1);
  });

  it('Add layer', async () => {
    pagComposition.addLayerAt(pagLayer, 1);
    expect(pagComposition.numChildren()).to.be.eq(numChildren);
  });

  it('Get contains', async () => {
    expect(pagComposition.contains(pagLayer)).to.be.eq(true);
  });

  let pagLayer_1: PAGLayer | null;
  let pagLayer_2: PAGLayer | null;
  it('Swap layer', async () => {
    pagLayer_1 = pagComposition.getLayerAt(0);
    pagLayer_2 = pagComposition.getLayerAt(1);
    pagComposition.swapLayer(pagLayer_1, pagLayer_2);
    expect(pagComposition.getLayerIndex(pagLayer_1)).to.be.eq(1);
    expect(pagComposition.getLayerIndex(pagLayer_2)).to.be.eq(0);
  });

  it('Swap layer', async () => {
    pagComposition.swapLayerAt(0, 1);
    expect(pagComposition.getLayerIndex(pagLayer_1)).to.be.eq(0);
    expect(pagComposition.getLayerIndex(pagLayer_2)).to.be.eq(1);
  });
  pagLayer_1?.destroy();
  pagLayer_1 = null;
  pagLayer_2?.destroy();
  pagLayer_2 = null;

  it('Get audio bytes', async () => {
    expect(ArrayBuffer.isView(pagComposition.audioBytes())).to.be.eq(true);
  });

  it('Get audio markers', async () => {
    expect(pagComposition.audioMarkers().size()).to.be.eq(2);
  });

  it('Get audio startTime', async () => {
    expect(pagComposition.audioStartTime()).to.be.eq(0);
  });

  it('Get layers by name', async () => {
    expect(pagComposition.getLayersByName('').size()).to.be.eq(0);
  });

  it('Get layers under point', async () => {
    expect(pagComposition.getLayersUnderPoint(1, 1).size()).to.be.eq(2);
  });

  it('Remove all layers', async () => {
    pagComposition.removeAllLayers();
    expect(pagComposition.numChildren()).to.be.eq(0);
  });
});
