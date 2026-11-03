/* eslint prefer-arrow-callback: ["error", { "allowNamedFunctions": true }] */
/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach afterEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGFile } from '../../src/pag-file';
import type { PAGLayer } from '../../src/pag-layer';
import type { Matrix } from '../../src/core/matrix';
import { visitTestPage } from './lane';

describe('PAGLayer', function layerTests() {
  let PAG: PAG;
  let PAGTypes: typeof Libpag.types;
  let pagFile: PAGFile;
  let pagLayer: PAGLayer;
  let ownedLayers: PAGLayer[];
  let ownedMatrices: Matrix[];
  let ownedVectors: { delete: () => void }[];

  function ownLayer<T extends PAGLayer>(layer: T): T {
    for (const ownedLayer of ownedLayers) {
      if (ownedLayer.wasmIns === layer.wasmIns) return layer;
    }
    ownedLayers.push(layer);
    return layer;
  }

  function ownMatrix(matrix: Matrix): Matrix {
    ownedMatrices.push(matrix);
    return matrix;
  }

  beforeEach(function setupLayer() {
    ownedLayers = [];
    ownedMatrices = [];
    ownedVectors = [];
    visitTestPage();
    cy.window().then(async function loadLayer(window: Cypress.AUTWindow & { libpag: typeof Libpag }) {
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
      const response = await window.fetch('/demo/assets/test2.pag');
      pagFile = ownLayer(await PAG.PAGFile.load(await response.arrayBuffer()));
      pagLayer = ownLayer(pagFile.getLayerAt(0));
    });
  });

  afterEach(function destroyLayer() {
    for (const vector of ownedVectors) {
      vector.delete();
    }
    for (const matrix of ownedMatrices.reverse()) {
      matrix.destroy();
    }
    for (const layer of ownedLayers.reverse()) {
      layer.destroy();
    }
  });

  it('Get uniqueID', function getUniqueID() {
    const uniqueID = pagLayer.uniqueID();
    const sameLayer = ownLayer(pagFile.getLayerAt(0));
    expect(uniqueID).to.be.greaterThan(0);
    expect(sameLayer.uniqueID()).to.be.eq(uniqueID);
    expect(pagFile.uniqueID()).not.to.be.eq(uniqueID);
  });

  it('Get layerType', function getLayerType() {
    expect(pagLayer.layerType()).to.be.eq(PAGTypes.LayerType.Solid);
  });

  it('Get layerName', function getLayerName() {
    expect(pagLayer.layerName()).to.be.eq('SolidLayer');
  });

  it('Get/Set matrix', function setMatrix() {
    const matrix = ownMatrix(pagLayer.matrix());
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 0, 0]);
    matrix.set(PAGTypes.MatrixIndex.a, 2);
    pagLayer.setMatrix(matrix);
    expect(ownMatrix(pagLayer.matrix()).a).to.be.eq(2);
  });

  it('Reset matrix', function resetMatrix() {
    const matrix = ownMatrix(pagLayer.matrix());
    matrix.set(PAGTypes.MatrixIndex.a, 2);
    pagLayer.setMatrix(matrix);
    expect(ownMatrix(pagLayer.matrix()).a).to.be.eq(2);
    pagLayer.resetMatrix();
    expect(ownMatrix(pagLayer.matrix()).a).to.be.eq(1);
  });

  it('Get total matrix', function getTotalMatrix() {
    const matrix = ownMatrix(pagLayer.getTotalMatrix());
    expect(matrix.wasmIns).to.be.a('object');
  });

  it('Get/Set alpha', function setAlpha() {
    expect(pagLayer.alpha()).to.be.eq(1);
    pagLayer.setAlpha(0);
    expect(pagLayer.alpha()).to.be.eq(0);
  });

  it('Get/Set visible', function setVisible() {
    expect(pagLayer.visible()).to.be.eq(true);
    pagLayer.setVisible(false);
    expect(pagLayer.visible()).to.be.eq(false);
  });

  it('Get editableIndex', function getEditableIndex() {
    expect(pagLayer.editableIndex()).to.be.eq(-1);
  });

  it('Get parent', function getParent() {
    const parent = ownLayer(pagLayer.parent());
    expect(parent.wasmIns).to.be.a('object');
    expect(parent.uniqueID()).to.be.eq(pagFile.uniqueID());
  });

  it('Get markers', function getMarkers() {
    const markers = pagLayer.markers();
    ownedVectors.push(markers);
    expect(markers.size()).to.be.eq(0);
  });

  it('localTime to global', function localTimeToGlobal() {
    expect(pagLayer.localTimeToGlobal(0)).to.be.eq(0);
  });

  it('global to localTime', function globalToLocalTime() {
    expect(pagLayer.globalToLocalTime(0)).to.be.eq(0);
  });

  it('Get duration', function getDuration() {
    expect(pagLayer.duration()).to.be.eq(4000000);
  });

  it('Get frameRate', function getFrameRate() {
    expect(pagLayer.frameRate()).to.be.eq(30);
  });

  it('Get/Set startTime', function setStartTime() {
    expect(pagLayer.startTime()).to.be.eq(0);
    pagLayer.setStartTime(1000000);
    expect(pagLayer.startTime()).to.be.eq(1000000);
  });

  it('Get/Set currentTime', function setCurrentTime() {
    expect(pagLayer.currentTime()).to.be.eq(0);
    pagLayer.setCurrentTime(1000000);
    expect(pagLayer.currentTime()).to.be.eq(1000000);
  });

  it('Get/Set progress', function setProgress() {
    expect(Math.floor(pagLayer.getProgress() * 1000) / 1000).to.be.eq(0);
    pagLayer.setProgress(0.5);
    expect(Math.floor(pagLayer.getProgress() * 1000) / 1000).to.be.eq(0.5);
  });

  it('preFrame', function previousFrame() {
    pagLayer.setProgress(0.5);
    const progress = pagLayer.getProgress();
    pagLayer.preFrame();
    expect(pagLayer.getProgress()).to.be.lessThan(progress);
  });

  it('nextFrame', function nextFrame() {
    pagLayer.setProgress(0.5);
    const progress = pagLayer.getProgress();
    pagLayer.nextFrame();
    expect(pagLayer.getProgress()).to.be.greaterThan(progress);
  });

  it('Get bounds', function getBounds() {
    expect(pagLayer.getBounds()).to.be.eql({ top: 0, right: 1076, bottom: 1920, left: 0 });
  });

  it('Get/Set excludedFromTimeline', function setExcludedFromTimeline() {
    expect(pagLayer.excludedFromTimeline()).to.be.eq(false);
    pagLayer.setExcludedFromTimeline(true);
    expect(pagLayer.excludedFromTimeline()).to.be.eq(true);
  });

  it('Is PAGFile', function isPAGFile() {
    expect(pagLayer.isPAGFile()).to.be.eq(false);
  });

  it('As TypeLayer', function asTypeLayer() {
    const typedLayer = pagLayer.asTypeLayer();
    expect(typedLayer.layerType()).to.be.eq(PAGTypes.LayerType.Solid);
    expect(typedLayer.uniqueID()).to.be.eq(pagLayer.uniqueID());
  });
});
