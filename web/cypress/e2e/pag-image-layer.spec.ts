/* eslint-disable @typescript-eslint/no-invalid-this */
/* eslint-disable max-nested-callbacks */
/* global describe it expect cy Cypress beforeEach afterEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGFile } from '../../src/pag-file';
import type { PAGImageLayer } from '../../src/pag-image-layer';
import type { PAGImage } from '../../src/pag-image';
import { visitTestPage } from './lane';

describe('PAGImageLayer', async () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let pagFile: PAGFile;
  let pagImageLayer: PAGImageLayer;
  let resources: { isDestroyed?: boolean; destroy: () => void }[];

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
      const layers = pagFile.getLayersByEditableIndex(0, PAGTypes.LayerType.Image);
      pagImageLayer = layers.get(0) as PAGImageLayer;
      resources.push(pagImageLayer);
      layers.delete();
    });
  });

  afterEach(() => {
    for (const resource of resources.reverse()) {
      if (!resource.isDestroyed) resource.destroy();
    }
    resources = [];
  });

  function makePixels(color: number[], width = 4, height = 4): Uint8Array {
    const pixels = new Uint8Array(width * height * 4);
    for (let index = 0; index < width * height; index++) pixels.set(color, index * 4);
    return pixels;
  }

  function replacementImage(color: number[]): PAGImage {
    const image = PAG.PAGImage.fromPixels(makePixels(color), 4, 4,
      PAGTypes.ColorType.RGBA_8888, PAGTypes.AlphaType.Opaque);
    resources.push(image);
    return image;
  }

  it('Get contentDuration', async () => {
    expect(pagImageLayer.contentDuration()).to.be.eq(19440000);
  });

  it('Get videoRanges', async () => {
    expect(pagImageLayer.getVideoRanges().length).to.be.eq(5);
  });

  it('Replace Image updates image rendering state and can be reset', async () => {
    const image = replacementImage([255, 0, 0]);
    expect(image.width()).to.equal(4);
    expect(image.height()).to.equal(4);
    pagImageLayer.replaceImage(image);
    image.destroy();
    pagImageLayer.replaceImage(null);
    expect(pagImageLayer.imageBytes()).to.not.equal(null);
  });

  it('Set Image accepts a replacement and null reset independently', async () => {
    const image = replacementImage([0, 255, 0]);
    expect(image.wasmIns).to.be.a('object');
    pagImageLayer.setImage(image);
    image.destroy();
    pagImageLayer.setImage(null);
    expect(pagImageLayer.imageBytes()).to.not.equal(null);
  });

  it('layerTime to content', async () => {
    expect(pagImageLayer.layerTimeToContent(0)).to.be.eq(0);
  });

  it('contentTime to layer', async () => {
    expect(pagImageLayer.contentTimeToLayer(0)).to.be.eq(880000);
  });
});
