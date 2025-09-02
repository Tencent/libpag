/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGPlayer } from '../../src/pag-player';
import type { PAGFile } from '../../src/pag-file';

describe('PAGPlayer', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let pagPlayer: PAGPlayer;

  beforeEach(() => {
    cy.visit('/cypress/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
    });
  });

  const createPAGPlayer = () => {
    const pagPlayer = PAG.PAGPlayer.create();
    expect(pagPlayer.wasmIns).to.be.a('Object');
    return pagPlayer;
  };

  const getPAGFile = async () => {
    const buffer = await global.fetch('/demo/assets/AudioMarker.pag').then((res) => res.arrayBuffer());
    const pagFile = await PAG.PAGFile.load(buffer);
    expect(pagFile.wasmIns).to.be.a('Object');
    return pagFile;
  };

  it('Create PAGPlayer', () => {
    createPAGPlayer();
  });

  it('PAGPlayer flush', async () => {
    pagPlayer = createPAGPlayer();
    pagPlayer.prepare();
    const res = await pagPlayer.flush();
    expect(res).to.be.eq(false);
  });

  it('Get/Set PAGComposition', async () => {
    pagPlayer = createPAGPlayer();
    const pagFile = await getPAGFile();
    pagPlayer.setComposition(pagFile);
    const compositon = pagPlayer.getComposition() as PAGFile;
    expect(compositon.wasmIns).to.be.a('Object');

    const imageBlob = await global.fetch('/demo/assets/cat.png').then((res) => res.blob());
    const pagImage = await PAG.PAGImage.fromFile(new File([imageBlob], 'cat.png'));
    expect(pagImage.wasmIns).to.be.a('object');
    compositon.replaceImage(1, pagImage);
  });

  it('Get/Set PAGSurface', async () => {
    pagPlayer = createPAGPlayer();
    const pagFile = await getPAGFile();
    pagPlayer.setComposition(pagFile);
    const pagSurface = PAG.PAGSurface.fromCanvas('#pag');
    expect(pagSurface.wasmIns).to.be.a('Object');
    pagPlayer.setSurface(pagSurface);
    expect(pagPlayer.getSurface().wasmIns).to.be.a('Object');
  });

  it('Get duration', async () => {
    expect(pagPlayer.duration()).to.be.eq(20000000);
  });

  it('Get/Set progress', async () => {
    expect(pagPlayer.getProgress()).to.be.eq(0.0002);
    pagPlayer.setProgress(0.5);
    expect(pagPlayer.getProgress()).to.be.eq(0.5002);
  });

  it('Get/Set videoEnabled', async () => {
    expect(pagPlayer.videoEnabled()).to.be.eq(true);
    pagPlayer.setVideoEnabled(false);
    expect(pagPlayer.videoEnabled()).to.be.eq(false);
  });

  it('Get/Set cacheEnabled', async () => {
    expect(pagPlayer.cacheEnabled()).to.be.eq(true);
    pagPlayer.setCacheEnabled(false);
    expect(pagPlayer.cacheEnabled()).to.be.eq(false);
  });

  it('Get/Set cacheScale', async () => {
    expect(pagPlayer.cacheScale()).to.be.eq(1);
    pagPlayer.setCacheScale(0.5);
    expect(pagPlayer.cacheScale()).to.be.eq(0.5);
  });

  it('Get/Set maxFrameRate', async () => {
    expect(pagPlayer.maxFrameRate()).to.be.eq(60);
    pagPlayer.setMaxFrameRate(30);
    expect(pagPlayer.maxFrameRate()).to.be.eq(30);
  });

  it('Get/Set scaleMode', async () => {
    expect(pagPlayer.scaleMode()).to.be.eq(PAGTypes.PAGScaleMode.LetterBox);
    pagPlayer.setScaleMode(PAGTypes.PAGScaleMode.Stretch);
    expect(pagPlayer.scaleMode()).to.be.eq(PAGTypes.PAGScaleMode.Stretch);
  });

  it('Get/Set matrix', () => {
    let matrix = pagPlayer.matrix();
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([
      0.4166666567325592, 0, 0, 0.234375, 0, 0,
    ]);
    matrix.set(PAGTypes.MatrixIndex.a, 2);
    pagPlayer.setMatrix(matrix);
    matrix = pagPlayer.matrix();
    expect(matrix.a).to.be.eq(2);
  });

  it('Next Frame', () => {
    pagPlayer.nextFrame();
    pagPlayer.prepare();
  });

  it('Get/Set autoClear', () => {
    expect(pagPlayer.autoClear()).to.be.eq(true);
    pagPlayer.setAutoClear(false);
    expect(pagPlayer.autoClear()).to.be.eq(false);
  });

  it('Get bounds', () => {
    const pagLayer = pagPlayer.getComposition().getLayerAt(0);
    expect(pagPlayer.getBounds(pagLayer)).to.be.eql({
      left: 0,
      top: 0,
      right: 1440,
      bottom: 300,
    });
  });

  it('Get layers under point', () => {
    expect(pagPlayer.getLayersUnderPoint(0, 0).size()).to.be.eq(3);
  });

  it('Hit test point', () => {
    const pagLayer = pagPlayer.getComposition().getLayerAt(0);
    expect(pagPlayer.hitTestPoint(pagLayer, 0, 0)).to.be.eq(true);
  });

  it('Get renderingTime', () => {
    expect(pagPlayer.renderingTime()).to.be.eq(0);
  });

  it('Get imageDecodingTime', () => {
    expect(pagPlayer.imageDecodingTime()).to.be.eq(0);
  });

  it('Get presentingTime', () => {
    expect(pagPlayer.presentingTime()).to.be.eq(0);
  });

  it('Get graphicsMemory', () => {
    expect(pagPlayer.graphicsMemory()).to.be.eq(0);
  });
});
