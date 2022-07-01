/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGFile } from '../../src/pag-file';
import type { PAGLayer } from '../../src/pag-layer';

describe('PAGLayer', async () => {
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

  let pagFile: PAGFile;
  let pagLayer: PAGLayer;

  it('Get uniqueID', async () => {
    const buffer = await global.fetch('http://127.0.0.1:8080/demo/assets/test2.pag').then((res) => res.arrayBuffer());
    pagFile = await PAG.PAGFile.load(buffer);
    pagLayer = pagFile.getLayerAt(0);
    expect(pagLayer.uniqueID()).to.be.eq(16);
  });

  it('Get layerType', async () => {
    expect(pagLayer.layerType()).to.be.eq(PAGTypes.LayerType.Solid);
  });

  it('Get layerName', async () => {
    expect(pagLayer.layerName()).to.be.eq('');
  });

  it('Get/Set matrix', () => {
    let matrix = pagLayer.matrix();
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 0, 0]);
    matrix.set(PAGTypes.MatrixIndex.a, 2);
    pagLayer.setMatrix(matrix);
    matrix = pagLayer.matrix();
    expect(matrix.a).to.be.eq(2);
  });

  it('Reset matrix', () => {
    pagLayer.resetMatrix();
    expect(pagLayer.matrix().a).to.be.eq(1);
  });

  it('Get total matrix', () => {
    const matrix = pagLayer.getTotalMatrix();
    expect(matrix.wasmIns).to.be.a('Object');
  });

  it('Get/Set alpha', () => {
    expect(pagLayer.alpha()).to.be.eq(1);
    pagLayer.setAlpha(0);
    expect(pagLayer.alpha()).to.be.eq(0);
  });

  it('Get/Set visible', () => {
    expect(pagLayer.visible()).to.be.eq(true);
    pagLayer.setVisible(false);
    expect(pagLayer.visible()).to.be.eq(false);
  });

  it('Get editableIndex', () => {
    expect(pagLayer.editableIndex()).to.be.eq(-1);
  });

  it('Get parent', () => {
    expect(pagLayer.parent().wasmIns).to.be.a('Object');
  });

  it('Get markers', () => {
    expect(pagLayer.markers().size()).to.be.eq(0);
  });

  it('localTime to global', () => {
    expect(pagLayer.localTimeToGlobal(0)).to.be.eq(0);
  });

  it('global to localTime', () => {
    expect(pagLayer.globalToLocalTime(0)).to.be.eq(0);
  });

  it('Get duration', () => {
    expect(pagLayer.duration()).to.be.eq(4000000);
  });

  it('Get frameRate', () => {
    expect(pagLayer.frameRate()).to.be.eq(30);
  });

  it('Get/Set startTime', () => {
    expect(pagLayer.startTime()).to.be.eq(0);
    pagLayer.setStartTime(1000000);
    expect(pagLayer.startTime()).to.be.eq(1000000);
  });

  it('Get/Set currentTime', () => {
    expect(pagLayer.currentTime()).to.be.eq(0);
    pagLayer.setCurrentTime(1000000);
    expect(pagLayer.currentTime()).to.be.eq(1000000);
  });

  it('Get/Set progress', () => {
    expect(Math.floor(pagLayer.getProgress() * 1000) / 1000).to.be.eq(0);
    pagLayer.setProgress(0.5);
    expect(Math.floor(pagLayer.getProgress() * 1000) / 1000).to.be.eq(0.5);
  });

  it('preFrame', () => {
    const progress = pagLayer.getProgress();
    pagLayer.preFrame();
    expect(pagLayer.getProgress() < progress).to.be.eq(true);
  });

  it('nextFrame', () => {
    const progress = pagLayer.getProgress();
    pagLayer.nextFrame();
    expect(pagLayer.getProgress() > progress).to.be.eq(true);
  });

  it('Get bounds', () => {
    expect(pagLayer.getBounds()).to.be.eql({ top: 0, right: 1076, bottom: 1920, left: 0 });
  });

  it('Get/Set excludedFromTimeline', () => {
    expect(pagLayer.excludedFromTimeline()).to.be.eq(false);
    pagLayer.setExcludedFromTimeline(true);
    expect(pagLayer.excludedFromTimeline()).to.be.eq(true);
  });

  it('Is PAGFile', () => {
    expect(pagLayer.isPAGFile()).to.be.eq(false);
  });
});
