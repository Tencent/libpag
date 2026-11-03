/* eslint prefer-arrow-callback: ["error", { "allowNamedFunctions": true }] */
/* eslint-disable max-nested-callbacks  */
/* global describe it expect cy Cypress beforeEach afterEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGView } from '../../src/pag-view';
import type { PAGFile } from '../../src/pag-file';
import { visitTestPage } from './lane';

describe('PAGView', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let pagView: PAGView;
  let pagFile: PAGFile;
  let resources: { isDestroyed?: boolean; destroy: () => void }[] = [];
  let snapshot: ImageBitmap | null = null;

  beforeEach(() => {
    resources = [];
    snapshot = null;
    visitTestPage();
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;

      const buffer = await global.fetch('/demo/assets/test.pag').then((res) => res.arrayBuffer());
      pagFile = await PAG.PAGFile.load(buffer);
      resources.push(pagFile);
      expect(pagFile.wasmIns).to.be.a('Object');
      const pagCanvas = global.document.getElementById('pag') as HTMLCanvasElement;
      // useScale keeps the canvas at its attribute size, so the initial matrix assertion below
      // stays independent of devicePixelRatio (Electron 130 headless follows the Retina screen
      // scale factor, unlike the old Cypress 9 Electron headless which reported 1).
      pagView = (await PAG.PAGView.init(pagFile, pagCanvas, { useScale: false })) as PAGView;
      resources.push(pagView);
    });
  });

  afterEach(async () => {
    if (resources.includes(pagView) && !pagView.isDestroyed) {
      for (const eventName of [
        'onAnimationStart',
        'onAnimationPlay',
        'onAnimationPause',
        'onAnimationCancel',
        'onAnimationUpdate',
      ] as const) {
        pagView.removeListener(eventName);
      }
      pagView.pause();
      await pagView.stop(false);
    }
    snapshot?.close();
    for (const resource of resources.reverse()) {
      if (!resource.isDestroyed) resource.destroy();
    }
    resources = [];
  });

  function controlPlayback() {
    let now = 0;
    let animationFrame: FrameRequestCallback | undefined;
    cy.stub(global.performance, 'now').callsFake(function currentTime() {
      return now;
    });
    cy.stub(global, 'requestAnimationFrame').callsFake(function requestFrame(callback: FrameRequestCallback) {
      animationFrame = callback;
      return 1;
    });
    cy.stub(global, 'cancelAnimationFrame').callsFake(function cancelFrame() {
      animationFrame = undefined;
    });
    return function advanceFrame(milliseconds: number) {
      now += milliseconds;
      const callback = animationFrame;
      animationFrame = undefined;
      callback?.(now);
    };
  }

  it('Get duration', () => {
    expect(pagView.duration()).to.be.eq(3000000);
  });

  it('Add Listener', () => {
    controlPlayback();
    const onPlay = cy.spy();
    const onPause = cy.spy();
    pagView.addListener('onAnimationPlay', onPlay);
    pagView.addListener('onAnimationPause', onPause);
    cy.then(async function notifyListeners() {
      await pagView.play();
      pagView.pause();
      expect(onPlay).to.have.been.calledOnceWithExactly(pagView);
      expect(onPause).to.have.been.calledOnceWithExactly(pagView);
      expect(onPlay).to.have.been.calledBefore(onPause);
    });
  });

  it('PAGView Play', () => {
    const advanceFrame = controlPlayback();
    const onStart = cy.spy();
    const onPlay = cy.spy();
    const onPause = cy.spy();
    const onCancel = cy.spy();
    const onUpdate = cy.spy().as('animationUpdate');
    pagView.addListener('onAnimationStart', onStart);
    pagView.addListener('onAnimationPlay', onPlay);
    pagView.addListener('onAnimationPause', onPause);
    pagView.addListener('onAnimationCancel', onCancel);
    pagView.addListener('onAnimationUpdate', onUpdate);
    const initialProgress = pagView.getProgress();
    let pausedProgress = initialProgress;
    cy.then(async function play() {
      pagView.setRepeatCount(1);
      await pagView.prepare();
      await pagView.play();
      expect(pagView.isPlaying).to.be.eq(true);
      expect(onStart).to.have.been.calledOnceWithExactly(pagView);
      expect(onPlay).to.have.been.calledOnceWithExactly(pagView);
      advanceFrame(500);
    });
    cy.get('@animationUpdate').should('have.callCount', 3);
    cy.then(function pause() {
      expect(pagView.currentFrame()).to.be.eq(Math.floor(pagFile.frameRate() / 2));
      pausedProgress = pagView.getProgress();
      expect(pausedProgress).to.be.greaterThan(initialProgress);
      pagView.pause();
      expect(pagView.isPlaying).to.be.eq(false);
      expect(onPause).to.have.been.calledOnceWithExactly(pagView);
      advanceFrame(500);
      expect(pagView.getProgress()).to.be.eq(pausedProgress);
    });
    cy.then(async function resume() {
      await pagView.play();
      expect(pagView.isPlaying).to.be.eq(true);
      expect(pagView.getProgress()).to.be.eq(pausedProgress);
      expect(onPlay.callCount).to.be.eq(2);
      expect(onStart.callCount).to.be.eq(1);
      advanceFrame(500);
    });
    cy.get('@animationUpdate').should('have.callCount', 5);
    cy.then(async function stop() {
      expect(pagView.currentFrame()).to.be.eq(pagFile.frameRate());
      expect(pagView.getProgress()).to.be.greaterThan(pausedProgress);
      await pagView.stop();
      expect(pagView.isPlaying).to.be.eq(false);
      expect(pagView.currentFrame()).to.be.eq(0);
      expect(pagView.getProgress()).to.be.eq(initialProgress);
      expect(onCancel).to.have.been.calledOnceWithExactly(pagView);
      expect(onPause).to.have.been.calledBefore(onCancel);
      advanceFrame(500);
      expect(pagView.getProgress()).to.be.eq(initialProgress);
    });
  });

  it('PAGView Pause', () => {
    controlPlayback();
    const onPause = cy.spy();
    pagView.addListener('onAnimationPause', onPause);
    cy.then(async function pausePlayingView() {
      pagView.setProgress(0.5);
      await pagView.play();
      const progress = pagView.getProgress();
      expect(pagView.isPlaying).to.be.eq(true);
      pagView.pause();
      expect(pagView.isPlaying).to.be.eq(false);
      expect(pagView.getProgress()).to.be.eq(progress);
      expect(onPause).to.have.been.calledOnceWithExactly(pagView);
      pagView.pause();
      expect(onPause.callCount).to.be.eq(1);
    });
  });

  it('PAGView Stop', () => {
    controlPlayback();
    const onCancel = cy.spy();
    pagView.addListener('onAnimationCancel', onCancel);
    const initialProgress = pagView.getProgress();
    cy.then(async function stopPlayingView() {
      pagView.setProgress(0.5);
      await pagView.play();
      expect(pagView.isPlaying).to.be.eq(true);
      expect(pagView.getProgress()).to.be.greaterThan(initialProgress);
      await pagView.stop();
      expect(pagView.isPlaying).to.be.eq(false);
      expect(pagView.currentFrame()).to.be.eq(0);
      expect(pagView.getProgress()).to.be.eq(initialProgress);
      expect(onCancel).to.have.been.calledOnceWithExactly(pagView);
    });
  });

  it('Get/Set Progress', () => {
    expect(pagView.getProgress()).to.be.closeTo(0, 0.02);
    pagView.setProgress(0.5);
    expect(pagView.getProgress()).to.be.closeTo(0.5, 0.02);
  });

  it('Get currentFrame', () => {
    expect(pagView.currentFrame()).to.be.eq(0);
    pagView.setProgress(0.5);
    expect(pagView.currentFrame()).to.be.eq(Math.floor((pagView.duration() / 2000000) * pagFile.frameRate()));
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
    const composition = pagView.getComposition() as PAGFile;
    resources.push(composition);
    expect(composition.uniqueID()).to.be.eq(pagFile.uniqueID());

    const imageBlob = await global.fetch('/demo/assets/cat.png').then((res) => res.blob());
    const pagImage = await PAG.PAGImage.fromFile(new File([imageBlob], 'cat.png'));
    resources.push(pagImage);
    expect(pagImage.wasmIns).to.be.a('object');
    composition.replaceImage(0, pagImage);
    expect(await pagView.flush()).to.be.eq(true);

    const buffer = await global.fetch('/demo/assets/AudioMarker.pag').then((res) => res.arrayBuffer());
    const newPagFile = await PAG.PAGFile.load(buffer);
    resources.push(newPagFile);
    expect(newPagFile.wasmIns).to.be.a('Object');
    expect(newPagFile.uniqueID()).not.to.be.eq(pagFile.uniqueID());
    pagView.setComposition(newPagFile);
    const updatedComposition = pagView.getComposition();
    resources.push(updatedComposition);
    expect(updatedComposition.uniqueID()).to.be.eq(newPagFile.uniqueID());
    expect(pagView.duration()).to.be.eq(20000000);
    await pagView.prepare();
    expect(await pagView.flush()).to.be.eq(true);
  });

  it('Get/Set Matrix', () => {
    const matrix = pagView.matrix();
    resources.push(matrix);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([0.5, 0, 0, 0.5, 0, 0]);
    matrix.set(PAGTypes.MatrixIndex.a, 2);
    pagView.setMatrix(matrix);
    const updatedMatrix = pagView.matrix();
    resources.push(updatedMatrix);
    expect(updatedMatrix.a).to.be.eq(2);
    expect(pagView.scaleMode()).to.be.eq(PAGTypes.PAGScaleMode.None);
  });

  it('Get layers under point', () => {
    const layers = pagView.getLayersUnderPoint(0, 0);
    resources.push(...layers);
    const count = layers.size();
    layers.delete();
    expect(count).to.be.eq(2);
  });

  it('PAGView updateSize', async () => {
    const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
    expect([canvas.width, canvas.height]).to.be.eql([300, 300]);
    canvas.width = 600;
    canvas.height = 450;
    pagView.updateSize();
    const matrix = pagView.matrix();
    resources.push(matrix);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([0.75, 0, 0, 0.75, 75, 0]);
    expect(await pagView.flush()).to.be.eq(true);
    snapshot = await pagView.makeSnapshot();
    expect([snapshot.width, snapshot.height]).to.be.eql([600, 450]);
  });

  it('PAGView makeSnapshot', async () => {
    snapshot = await pagView.makeSnapshot();
    expect(snapshot).to.be.a('ImageBitmap');
    expect([snapshot.width, snapshot.height]).to.be.eql([300, 300]);
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
    controlPlayback();
    const removedPlay = cy.spy();
    const remainingPlay = cy.spy();
    const removedPause = cy.spy();
    pagView.addListener('onAnimationPlay', removedPlay);
    pagView.addListener('onAnimationPlay', remainingPlay);
    pagView.addListener('onAnimationPause', removedPause);
    expect(pagView.removeListener('onAnimationPlay', removedPlay)).to.be.eq(true);
    expect(pagView.removeListener('onAnimationPlay', removedPlay)).to.be.eq(false);
    expect(pagView.removeListener('onAnimationPause')).to.be.eq(true);
    cy.then(async function verifyRemovedListeners() {
      await pagView.play();
      pagView.pause();
      expect(removedPlay.callCount).to.be.eq(0);
      expect(removedPause.callCount).to.be.eq(0);
      expect(remainingPlay).to.have.been.calledOnceWithExactly(pagView);
      expect(pagView.removeListener('onAnimationPlay')).to.be.eq(true);
      await pagView.play();
      expect(removedPlay.callCount).to.be.eq(0);
      expect(remainingPlay.callCount).to.be.eq(1);
    });
  });

  it('PAGView destroy', () => {
    expect(pagView.isDestroyed).to.be.eq(false);
    pagView.destroy();
    expect(pagView.isDestroyed).to.be.eq(true);
  });
});
