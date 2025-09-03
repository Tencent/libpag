/* eslint-disable @typescript-eslint/no-invalid-this */
/* eslint-disable max-nested-callbacks  */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGView } from '../../src/pag-view';
import type { PAGTextLayer } from '../../src/pag-text-layer';
import { PAGFile } from '../../src/pag-file';

describe('PAGView', async () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let pagView: PAGView;

  beforeEach(() => {
    cy.visit('/cypress/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;

      const buffer = await global.fetch('/demo/assets/test.pag').then((res) => res.arrayBuffer());
      const pagFile = await PAG.PAGFile.load(buffer);
      expect(pagFile.wasmIns).to.be.a('Object');
      const pagCanvas = global.document.getElementById('pag') as HTMLCanvasElement;
      pagView = (await PAG.PAGView.init(pagFile, pagCanvas)) as PAGView;
    });
  });

  it('Get duration', () => {
    expect(pagView.duration()).to.be.eq(3000000);
  });

  it('Add Listener', () => {
    pagView.addListener('onAnimationPlay', (event) => {
      console.log('onAnimationPlay', event);
    });
    pagView.addListener('onAnimationPause', (event) => {
      console.log('onAnimationPause', event);
    });
  });

  it('PAGView Play', () => {
    pagView.setRepeatCount(1);
    pagView.prepare();
    pagView.play();
  });

  it('PAGView Pause', () => {
    pagView.pause();
  });

  it('PAGView Stop', () => {
    pagView.stop();
  });

  it('Get/Set Progress', () => {
    pagView.getProgress();
    pagView.setProgress(0.5);
    expect(pagView.getProgress()).to.be.closeTo(0.5, 0.02);
  });

  it('Get currentFrame', () => {
    pagView.currentFrame();
  });

  it('Get/Set videoEnabled', () => {
    expect(pagView.videoEnabled()).to.be.eq(true);
    pagView.setVideoEnabled(false);
    expect(pagView.videoEnabled()).to.be.eq(false);
  });

  it('Get/Set cacheEnabled', () => {
    expect(pagView.cacheEnabled()).to.be.eq(true);
    pagView.setCacheEnabled(false);
    expect(pagView.cacheEnabled()).to.be.eq(false);
  });

  it('Get/Set cacheScale', () => {
    expect(pagView.cacheScale()).to.be.closeTo(1.0, 0.02);
    pagView.setCacheScale(0.5);
    expect(pagView.cacheScale()).to.be.closeTo(0.5, 0.02);
  });

  it('Get/Set maxFrameRate', () => {
    expect(pagView.maxFrameRate()).to.be.eq(60);
    pagView.setMaxFrameRate(70);
    expect(pagView.maxFrameRate()).to.be.eq(70);
  });

  it('Get/Set scaleMode', () => {
    expect(pagView.scaleMode()).to.be.eq(PAGTypes.PAGScaleMode.LetterBox);
    pagView.setScaleMode(PAGTypes.PAGScaleMode.Stretch);
    expect(pagView.scaleMode()).to.be.eq(PAGTypes.PAGScaleMode.Stretch);
  });

  it('PAGView flush', async () => {
    const res = await pagView.flush();
    expect(res).to.be.eq(false);
  });

  it('PAGView freeCache', () => {
    pagView.freeCache();
  });

  it('Get/Set Composition', async () => {
    const pagFile = pagView.getComposition() as PAGFile;
    expect(pagFile.wasmIns).to.be.a('object');

    const imageBlob = await global.fetch('/demo/assets/cat.png').then((res) => res.blob());
    const pagImage = await PAG.PAGImage.fromFile(new File([imageBlob], 'cat.png'));
    expect(pagImage.wasmIns).to.be.a('object');
    pagFile.replaceImage(0, pagImage);

    const buffer = await global.fetch('/demo/assets/AudioMarker.pag').then((res) => res.arrayBuffer());
    const newPagFile = await PAG.PAGFile.load(buffer);
    expect(pagFile.wasmIns).to.be.a('Object');
    pagView.setComposition(newPagFile);
  });

  it('Get/Set Matrix', () => {
    let matrix = pagView.matrix();
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([0.5, 0, 0, 0.5, 0, 0]);
    matrix.set(PAGTypes.MatrixIndex.a, 2);
    pagView.setMatrix(matrix);
    matrix = pagView.matrix();
    expect(matrix.a).to.be.eq(2);
  });

  it('Get layers under point', () => {
    expect(pagView.getLayersUnderPoint(0, 0).size()).to.be.eq(2);
  });

  it('PAGView updateSize', () => {
    pagView.updateSize();
  });

  it('PAGView makeSnapshot', async () => {
    const image = await pagView.makeSnapshot();
    expect(image).to.be.a('ImageBitmap');
  });

  it('Get/Set DebugData', () => {
    const data = {
      FPS: 1,
      flushTime: 2,
    };
    pagView.setDebugData(data);
    let debugData = pagView.getDebugData();
    expect([debugData.FPS, debugData.flushTime]).to.be.eql([data.FPS, data.flushTime]);
  });

  it('Remove Listener', () => {
    pagView.removeListener('onAnimationPlay');
    pagView.removeListener('onAnimationPause');
  });

  it('PAGView destroy', () => {
    pagView.destroy();
  });
});
