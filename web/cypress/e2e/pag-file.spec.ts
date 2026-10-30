/* eslint prefer-arrow-callback: ["error", { "allowNamedFunctions": true }] */
/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach afterEach */
import type * as Libpag from '../../src/pag';
import type { PAG, TextDocument, Vector } from '../../src/types';
import type { PAGFile } from '../../src/pag-file';
import type { PAGImage } from '../../src/pag-image';
import type { PAGLayer } from '../../src/pag-layer';
import type { PAGTextLayer } from '../../src/pag-text-layer';
import { visitTestPage } from './lane';

const textDataBaseline = {
  applyFill: true,
  applyStroke: true,
  baselineShift: 0,
  boxText: true,
  boxTextPos: {
    x: -313,
    y: -73,
  },
  boxTextSize: {
    x: 796,
    y: 312,
  },
  firstBaseLine: 15.1982421875,
  fauxBold: false,
  fauxItalic: true,
  fillColor: {
    red: 231,
    green: 35,
    blue: 86,
  },
  fontFamily: 'Yuanti SC',
  fontStyle: 'Regular',
  fontSize: 120,
  strokeColor: {
    red: 235,
    green: 235,
    blue: 235,
  },
  strokeOverFill: false,
  strokeWidth: 6,
  text: '过去的一年里最开心的瞬间',
  justification: 1,
  leading: 144,
  tracking: 0,
  backgroundColor: {
    red: 255,
    green: 255,
    blue: 255,
  },
  backgroundAlpha: 0,
  direction: 0,
};

describe('PAGFile', function fileTests() {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let pagFile: PAGFile;
  let ownedLayers: PAGLayer[];
  let ownedImages: PAGImage[];
  let ownedTextData: TextDocument[];
  let ownedVectors: Vector<PAGLayer>[];

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

  beforeEach(function setupFile() {
    ownedLayers = [];
    ownedImages = [];
    ownedTextData = [];
    ownedVectors = [];
    visitTestPage();
    cy.window().then(async function loadFile(window: Cypress.AUTWindow & { libpag: typeof Libpag }) {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
      const response = await window.fetch('/demo/assets/test2.pag');
      pagFile = ownLayer(await PAG.PAGFile.load(await response.arrayBuffer()));
    });
  });

  afterEach(function destroyFile() {
    for (const vector of ownedVectors) {
      vector.delete();
    }
    for (const textData of ownedTextData) {
      textData.delete();
    }
    for (const layer of ownedLayers.reverse()) {
      layer.destroy();
    }
    for (const image of ownedImages.reverse()) {
      image.destroy();
    }
  });

  it('Get maxSupportedTagLevel', function getMaxSupportedTagLevel() {
    expect(PAG.PAGFile.maxSupportedTagLevel()).to.be.greaterThan(83);
  });

  it('Load from arrayBuffer', function loadFromArrayBuffer() {
    expect(pagFile.wasmIns).to.be.a('object');
  });

  it('Get tagLevel', function getTagLevel() {
    expect(pagFile.tagLevel()).to.be.eq(52);
  });

  it('Get layer number', function getLayerNumber() {
    expect(pagFile.numTexts()).to.be.eq(1);
    expect(pagFile.numImages()).to.be.eq(0);
    expect(pagFile.numVideos()).to.be.eq(0);
  });

  it('Get textData', function getTextData() {
    const textData = pagFile.getTextData(0);
    ownedTextData.push(textData);
    for (const key of Object.keys(textDataBaseline)) {
      expect(textData[`${key}`]).to.be.eql(textDataBaseline[`${key}`]);
    }
  });

  it('Get layers by editableIndex', function getLayersByEditableIndex() {
    const layers = ownLayerVector(pagFile.getLayersByEditableIndex(0, PAGTypes.LayerType.Text));
    expect(layers.size()).to.be.eq(1);
  });

  it('Replace text', function replaceText() {
    const textData = pagFile.getTextData(0);
    ownedTextData.push(textData);
    const layers = ownLayerVector(pagFile.getLayersByEditableIndex(0, PAGTypes.LayerType.Text));
    const layer = layers.get(0) as PAGTextLayer;
    textData.text = 'test';
    pagFile.replaceText(0, textData);
    expect(layer.text()).to.be.eq('test');
  });

  it('Replace Image', async function replaceImage() {
    const response = await global.fetch('/demo/assets/test.pag');
    const imageFile = ownLayer(await PAG.PAGFile.load(await response.arrayBuffer()));
    expect(imageFile.wasmIns).to.be.a('object');
    const imageResponse = await global.fetch('/demo/assets/cat.png');
    const imageBlob = await imageResponse.blob();
    const image = await PAG.PAGImage.fromFile(new global.File([imageBlob], 'cat.png'));
    ownedImages.push(image);
    expect(image.wasmIns).to.be.a('object');
    imageFile.replaceImage(0, image);
  });

  it('Get Layers By EditableIndex', function getEditableTextLayers() {
    const layers = ownLayerVector(pagFile.getLayersByEditableIndex(0, PAGTypes.LayerType.Text));
    expect(layers.size()).to.be.eq(1);
  });

  it('Get editable indices', function getEditableIndices() {
    expect(pagFile.getEditableIndices(PAGTypes.LayerType.Text)).to.be.eql([0]);
  });

  it('Set/Get timeStretchMode', function setTimeStretchMode() {
    pagFile.setTimeStretchMode(PAGTypes.PAGTimeStretchMode.Scale);
    expect(pagFile.timeStretchMode()).to.be.eq(PAGTypes.PAGTimeStretchMode.Scale);
  });

  it('Set/Get duration', function setDuration() {
    pagFile.setDuration(1000000);
    expect(pagFile.duration()).to.be.eq(1000000);
  });

  it('Copy original', function copyOriginal() {
    const originalDuration = pagFile.duration();
    pagFile.setDuration(1000000);
    const original = ownLayer(pagFile.copyOriginal());
    expect(original.wasmIns).to.be.a('object');
    expect(original.duration()).to.be.eq(originalDuration);
    expect(pagFile.duration()).to.be.eq(1000000);
  });
});
