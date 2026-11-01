/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach afterEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGPlayer } from '../../src/pag-player';
import type { PAGFile } from '../../src/pag-file';
import { visitTestPage } from './lane';

describe('PAGPlayer', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let pagPlayer: PAGPlayer;
  let pagFile: PAGFile;
  let resources: { destroy: () => void; isDestroyed?: boolean }[] = [];

  beforeEach(() => {
    resources = [];
    visitTestPage();
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
      const buffer = await global.fetch('/demo/assets/AudioMarker.pag').then((res) => res.arrayBuffer());
      pagFile = await PAG.PAGFile.load(buffer);
      resources.push(pagFile);
      const pagSurface = PAG.PAGSurface.fromCanvas('#pag');
      resources.push(pagSurface);
      pagPlayer = PAG.PAGPlayer.create();
      resources.push(pagPlayer);
      pagPlayer.setComposition(pagFile);
      pagPlayer.setSurface(pagSurface);
      pagPlayer.setProgress(0);
    });
  });

  afterEach(() => {
    for (const resource of resources.reverse()) {
      if (!resource.isDestroyed) resource.destroy();
    }
    resources = [];
  });

  it('Create PAGPlayer', () => {
    expect(pagPlayer.wasmIns).to.be.a('Object');
    expect(pagPlayer.isDestroyed).to.be.eq(false);
  });

  it('PAGPlayer flush', async () => {
    await pagPlayer.prepare();
    expect(await pagPlayer.flush()).to.be.eq(true);
    expect(await pagPlayer.flush()).to.be.eq(false);
    pagPlayer.setComposition(null);
    await pagPlayer.prepare();
    await pagPlayer.flush();
    expect(await pagPlayer.flush()).to.be.eq(false);
  });

  it('Keeps other layers rendering when video preparation fails', async () => {
    const composition = PAG.PAGComposition.make(300, 300);
    const matrix = PAG.Matrix.makeTrans(100, 0);
    const solid = PAG.PAGSolidLayer.make(20000000, 40, 40, { red: 255, green: 0, blue: 0 }, 255);
    resources.push(composition, matrix, solid);
    pagPlayer.setComposition(null);
    pagFile.setMatrix(matrix);
    composition.addLayer(pagFile);
    composition.addLayer(solid);
    pagPlayer.setComposition(composition);
    pagPlayer.setScaleMode(PAGTypes.PAGScaleMode.None);

    async function readMarkerPixel(): Promise<number[]> {
      const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
      const image = await global.createImageBitmap(canvas);
      try {
        const probe = global.document.createElement('canvas');
        probe.width = 300;
        probe.height = 300;
        const context = probe.getContext('2d')!;
        context.drawImage(image, 0, 0);
        return Array.from(context.getImageData(10, 10, 1, 1).data);
      } finally {
        image.close();
      }
    }

    expect(await pagPlayer.flush()).to.equal(true);
    expect(await readMarkerPixel()).to.deep.equal([255, 0, 0, 255]);
    expect(pagPlayer.videoReaders.length).to.equal(1);
    const reader = pagPlayer.videoReaders[0];
    const failedPreparation = cy.stub(reader, 'prepare').resolves(false);
    const readError = cy.stub(reader, 'getError').resolves(new Error('Video preparation failed'));
    try {
      solid.setSolidColor({ red: 0, green: 255, blue: 0 });
      expect(await pagPlayer.flush()).to.equal(true);
      expect(failedPreparation.callCount).to.equal(1);
      expect(await readMarkerPixel()).to.deep.equal([0, 255, 0, 255]);
      expect(await reader.getError()).to.be.an('error');
    } finally {
      failedPreparation.restore();
      readError.restore();
    }
    pagPlayer.setProgress(0.5);
    expect(await pagPlayer.flush()).to.equal(true);
    expect(await reader.getError()).to.equal(null);
    expect(await pagPlayer.flush()).to.equal(false);
  });

  it('Prepares a nonzero video frame without starting playback', async () => {
    pagPlayer.setProgress(0.5);
    const progress = pagPlayer.getProgress();
    await pagPlayer.prepare();
    expect(pagPlayer.videoReaders.length).to.equal(pagFile.numVideos());
    expect(pagPlayer.isVideoPlaying()).to.equal(false);
    for (const reader of pagPlayer.videoReaders) {
      expect(reader.isPlaying).to.equal(false);
      expect(await reader.getError()).to.equal(null);
    }
    expect(pagPlayer.getProgress()).to.equal(progress);
    expect(await pagPlayer.flush()).to.equal(true);
    expect(await pagPlayer.flush()).to.equal(false);
  });

  it('Serializes simultaneous prepare and flush without duplicating video readers', async () => {
    const results = await Promise.all([pagPlayer.prepare(), pagPlayer.flush(), pagPlayer.flush()]);
    expect(results).to.deep.equal([undefined, true, false]);
    expect(pagPlayer.videoReaders.length).to.equal(pagFile.numVideos());
  });

  it('Cancels preparation when its composition is replaced', async () => {
    const pending = pagPlayer.prepare();
    const replacement = PAG.PAGComposition.make(300, 300);
    const solid = PAG.PAGSolidLayer.make(1000000, 300, 300, { red: 255, green: 0, blue: 0 }, 255);
    resources.push(replacement, solid);
    replacement.addLayer(solid);
    pagPlayer.setComposition(replacement);
    await pending;
    await pagPlayer.prepare();
    expect(pagPlayer.videoReaders.length).to.equal(0);
    expect(await pagPlayer.flush()).to.equal(true);
    expect(await pagPlayer.flush()).to.equal(false);
  });

  it('Settles active and queued operations when destroyed', async () => {
    const preparing = pagPlayer.prepare();
    const flushing = pagPlayer.flush();
    pagPlayer.destroy();
    await preparing;
    expect(await flushing).to.equal(false);
    expect(pagPlayer.isDestroyed).to.equal(true);
  });

  it('Get/Set PAGComposition', async () => {
    const replacement = pagFile.copyOriginal();
    resources.push(replacement);
    expect(replacement.uniqueID()).not.to.be.eq(pagFile.uniqueID());
    pagPlayer.setComposition(replacement);
    const composition = pagPlayer.getComposition() as PAGFile;
    resources.push(composition);
    expect(composition.uniqueID()).to.be.eq(replacement.uniqueID());
    expect(pagPlayer.duration()).to.be.eq(replacement.duration());

    const imageBlob = await global.fetch('/demo/assets/cat.png').then((res) => res.blob());
    const pagImage = await PAG.PAGImage.fromFile(new File([imageBlob], 'cat.png'));
    resources.push(pagImage);
    expect(pagImage.wasmIns).to.be.a('object');
    composition.replaceImage(1, pagImage);
  });

  it('Get/Set PAGSurface', () => {
    pagPlayer.setSurface(null);
    const pagSurface = PAG.PAGSurface.fromCanvas('#pag');
    resources.push(pagSurface);
    expect(pagSurface.wasmIns).to.be.a('Object');
    pagPlayer.setSurface(pagSurface);
    const actualSurface = pagPlayer.getSurface();
    resources.push(actualSurface);
    expect(actualSurface.wasmIns).to.be.a('Object');
    expect(actualSurface.width()).to.be.eq(300);
    expect(actualSurface.height()).to.be.eq(300);
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
    pagPlayer.setScaleMode(PAGTypes.PAGScaleMode.Stretch);
    const matrix = pagPlayer.matrix();
    resources.push(matrix);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([
      0.4166666567325592, 0, 0, 0.234375, 0, 0,
    ]);
    matrix.set(PAGTypes.MatrixIndex.a, 2);
    pagPlayer.setMatrix(matrix);
    const updatedMatrix = pagPlayer.matrix();
    resources.push(updatedMatrix);
    expect(updatedMatrix.a).to.be.eq(2);
    expect(pagPlayer.scaleMode()).to.be.eq(PAGTypes.PAGScaleMode.None);
  });

  it('Next Frame', async () => {
    expect(pagPlayer.currentFrame()).to.be.eq(0);
    const progress = pagPlayer.getProgress();
    pagPlayer.nextFrame();
    await pagPlayer.prepare();
    expect(pagPlayer.currentFrame()).to.be.eq(1);
    expect(pagPlayer.getProgress()).to.be.greaterThan(progress);
  });

  it('Get/Set autoClear', () => {
    expect(pagPlayer.autoClear()).to.be.eq(true);
    pagPlayer.setAutoClear(false);
    expect(pagPlayer.autoClear()).to.be.eq(false);
  });

  it('Get bounds', () => {
    pagPlayer.setScaleMode(PAGTypes.PAGScaleMode.Stretch);
    pagPlayer.setProgress(0.5);
    const pagLayer = pagFile.getLayerAt(0);
    resources.push(pagLayer);
    expect(pagPlayer.getBounds(pagLayer)).to.be.eql({
      left: 0,
      top: 0,
      right: 300,
      bottom: 300,
    });
  });

  it('Get layers under point', () => {
    pagPlayer.setScaleMode(PAGTypes.PAGScaleMode.Stretch);
    pagPlayer.setProgress(0.5);
    const layers = pagPlayer.getLayersUnderPoint(0, 0);
    resources.push(...layers);
    const count = layers.size();
    layers.delete();
    expect(count).to.be.eq(3);
  });

  it('Hit test point', () => {
    pagPlayer.setScaleMode(PAGTypes.PAGScaleMode.Stretch);
    pagPlayer.setProgress(0.5);
    const pagLayer = pagFile.getLayerAt(0);
    resources.push(pagLayer);
    expect(pagPlayer.hitTestPoint(pagLayer, 0, 0)).to.be.eq(true);
    expect(pagPlayer.hitTestPoint(pagLayer, -1, -1)).to.be.eq(false);
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
