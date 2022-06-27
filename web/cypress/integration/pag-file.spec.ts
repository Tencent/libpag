/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGFile } from '../../src/pag-file';
import type { PAGTextLayer } from '../../src/pag-text-layer';

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

describe('PAGFile', () => {
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

  it('Get maxSupportedTagLevel', () => {
    expect(PAG.PAGFile.maxSupportedTagLevel()).to.be.eq(83);
  });

  let pagFile: PAGFile;
  it('Load from arrayBuffer', async () => {
    const buffer = await global.fetch('http://127.0.0.1:8080/demo/assets/test2.pag').then((res) => res.arrayBuffer());
    pagFile = await PAG.PAGFile.load(buffer);
    expect(pagFile.wasmIns).to.be.a('object');
  });

  it('Get tagLevel', async () => {
    expect(pagFile.tagLevel()).to.be.eq(29);
  });

  it('Get layer number', async () => {
    expect(pagFile.numTexts()).to.be.eq(1);
    expect(pagFile.numImages()).to.be.eq(0);
    expect(pagFile.numVideos()).to.be.eq(0);
  });

  let textData: Libpag.types.TextDocument;
  it('Get textData', async () => {
    textData = pagFile.getTextData(0);
    Object.keys(textDataBaseline).forEach((key) => {
      expect(textData[`${key}`]).to.be.eql(textDataBaseline[`${key}`]);
    });
  });

  let pagLayers: Libpag.types.Vector<PAGTextLayer>;
  it('Get layers by editableIndex', () => {
    pagLayers = pagFile.getLayersByEditableIndex(0, PAGTypes.LayerType.Text) as Libpag.types.Vector<PAGTextLayer>;
    expect(pagLayers.size()).to.be.eq(1);
  });

  it('Replace text', () => {
    textData.text = 'test';
    pagFile.replaceText(0, textData);
    const pagLayer = pagLayers.get(0);
    expect(pagLayer.text()).to.be.eq('test');
  });

  it('Get editable indices', () => {
    expect(pagFile.getEditableIndices(PAGTypes.LayerType.Text)).to.be.eql([0]);
  });

  it('Set/Get timeStretchMode', () => {
    pagFile.setTimeStretchMode(PAGTypes.PAGTimeStretchMode.Scale);
    expect(pagFile.timeStretchMode()).to.be.eq(PAGTypes.PAGTimeStretchMode.Scale);
  });

  it('Set/Get duration', () => {
    pagFile.setDuration(1000000);
    expect(pagFile.duration()).to.be.eq(1000000);
  });

  it('Copy original', () => {
    expect(pagFile.copyOriginal().wasmIns).to.be.a('object');
  });
});
