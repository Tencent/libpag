/* eslint prefer-arrow-callback: ["error", { "allowNamedFunctions": true }] */
/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach afterEach */
import type * as Libpag from '../../src/pag';
import type { PAGComposition } from '../../src/pag-composition';
import type { PAGLayer } from '../../src/pag-layer';
import type { PAG, Vector } from '../../src/types';
import { visitTestPage } from './lane';

describe('PAGComposition', function compositionTests() {
  let PAG: PAG;
  let pagComposition: PAGComposition;
  let ownedLayers: PAGLayer[];
  let ownedVectors: { delete: () => void }[];

  function ownLayer<T extends PAGLayer>(layer: T): T {
    for (const ownedLayer of ownedLayers) {
      if (ownedLayer.wasmIns === layer.wasmIns) return layer;
    }
    ownedLayers.push(layer);
    return layer;
  }

  function ownLayerVector<T extends Vector<PAGLayer>>(layers: T): T {
    ownedVectors.push(layers);
    for (let index = 0; index < layers.size(); index += 1) {
      ownLayer(layers.get(index));
    }
    return layers;
  }

  beforeEach(function setupComposition() {
    ownedLayers = [];
    ownedVectors = [];
    visitTestPage();
    cy.window().then(async function loadComposition(window: Cypress.AUTWindow & { libpag: typeof Libpag }) {
      PAG = await window.libpag.PAGInit();
      const response = await window.fetch('/demo/assets/AudioMarker.pag');
      pagComposition = ownLayer(await PAG.PAGFile.load(await response.arrayBuffer()));
    });
  });

  afterEach(function destroyComposition() {
    for (const vector of ownedVectors) {
      vector.delete();
    }
    for (const layer of ownedLayers.reverse()) {
      layer.destroy();
    }
  });

  it('Make', function makeComposition() {
    const composition = ownLayer(PAG.PAGComposition.make(100, 100));
    expect(composition.wasmIns).to.be.a('object');
    expect(composition.width()).to.be.eq(100);
    expect(composition.height()).to.be.eq(100);
  });

  it('Get size', function getSize() {
    expect(pagComposition.wasmIns).to.be.a('object');
    expect(pagComposition.width()).to.be.eq(720);
    expect(pagComposition.height()).to.be.eq(1280);
  });

  it('Set content size', function setContentSize() {
    pagComposition.setContentSize(1000, 1000);
    expect(pagComposition.width()).to.be.eq(1000);
    expect(pagComposition.height()).to.be.eq(1000);
  });

  it('Get children number', function getChildrenNumber() {
    expect(pagComposition.numChildren()).to.be.eq(3);
  });

  it('Get layer at index', function getLayerAtIndex() {
    const layer = ownLayer(pagComposition.getLayerAt(0));
    expect(layer.wasmIns).to.be.a('object');
  });

  it('Get layer index', function getLayerIndex() {
    const layer = ownLayer(pagComposition.getLayerAt(0));
    expect(pagComposition.getLayerIndex(layer)).to.be.eq(0);
  });

  it('Set layer index', function setLayerIndex() {
    const layer = ownLayer(pagComposition.getLayerAt(0));
    pagComposition.setLayerIndex(layer, 1);
    expect(pagComposition.getLayerIndex(layer)).to.be.eq(1);
  });

  it('Remove layer', function removeLayer() {
    const numChildren = pagComposition.numChildren();
    const layer = ownLayer(pagComposition.getLayerAt(0));
    const removedLayer = ownLayer(pagComposition.removeLayer(layer));
    expect(removedLayer.uniqueID()).to.be.eq(layer.uniqueID());
    expect(pagComposition.numChildren()).to.be.eq(numChildren - 1);
    expect(pagComposition.contains(layer)).to.be.eq(false);
  });

  it('Add layer', function addLayer() {
    const numChildren = pagComposition.numChildren();
    const layer = ownLayer(pagComposition.removeLayerAt(0));
    expect(pagComposition.numChildren()).to.be.eq(numChildren - 1);
    expect(pagComposition.addLayer(layer)).to.be.eq(true);
    expect(pagComposition.numChildren()).to.be.eq(numChildren);
    expect(pagComposition.contains(layer)).to.be.eq(true);
  });

  it('Remove layer at index', function removeLayerAtIndex() {
    const numChildren = pagComposition.numChildren();
    const layer = ownLayer(pagComposition.getLayerAt(2));
    const removedLayer = ownLayer(pagComposition.removeLayerAt(2));
    expect(removedLayer.uniqueID()).to.be.eq(layer.uniqueID());
    expect(pagComposition.numChildren()).to.be.eq(numChildren - 1);
  });

  it('Add layer at index', function addLayerAtIndex() {
    const numChildren = pagComposition.numChildren();
    const layer = ownLayer(pagComposition.removeLayerAt(2));
    expect(pagComposition.numChildren()).to.be.eq(numChildren - 1);
    expect(pagComposition.addLayerAt(layer, 1)).to.be.eq(true);
    expect(pagComposition.numChildren()).to.be.eq(numChildren);
    expect(pagComposition.getLayerIndex(layer)).to.be.eq(1);
  });

  it('Get contains', function getContains() {
    const layer = ownLayer(pagComposition.getLayerAt(0));
    expect(pagComposition.contains(layer)).to.be.eq(true);
  });

  it('Swap layer', function swapLayer() {
    const firstLayer = ownLayer(pagComposition.getLayerAt(0));
    const secondLayer = ownLayer(pagComposition.getLayerAt(1));
    pagComposition.swapLayer(firstLayer, secondLayer);
    expect(pagComposition.getLayerIndex(firstLayer)).to.be.eq(1);
    expect(pagComposition.getLayerIndex(secondLayer)).to.be.eq(0);
  });

  it('Swap layer AtIndex', function swapLayerAtIndex() {
    const firstLayer = ownLayer(pagComposition.getLayerAt(0));
    const secondLayer = ownLayer(pagComposition.getLayerAt(1));
    pagComposition.swapLayerAt(0, 1);
    expect(pagComposition.getLayerIndex(firstLayer)).to.be.eq(1);
    expect(pagComposition.getLayerIndex(secondLayer)).to.be.eq(0);
  });

  it('Get audio bytes', function getAudioBytes() {
    expect(ArrayBuffer.isView(pagComposition.audioBytes())).to.be.eq(true);
  });

  it('Get audio markers', function getAudioMarkers() {
    const markers = pagComposition.audioMarkers();
    ownedVectors.push(markers);
    expect(markers.size()).to.be.eq(2);
  });

  it('Get audio startTime', function getAudioStartTime() {
    expect(pagComposition.audioStartTime()).to.be.eq(0);
  });

  it('Get layers by name', function getLayersByName() {
    const layers = ownLayerVector(pagComposition.getLayersByName('ImageLayer1'));
    expect(layers.size()).to.be.eq(1);
  });

  it('Get layers under point', function getLayersUnderPoint() {
    const layers = ownLayerVector(pagComposition.getLayersUnderPoint(1, 1));
    expect(layers.size()).to.be.eq(2);
  });

  it('Remove all layers', function removeAllLayers() {
    pagComposition.removeAllLayers();
    expect(pagComposition.numChildren()).to.be.eq(0);
  });
});
