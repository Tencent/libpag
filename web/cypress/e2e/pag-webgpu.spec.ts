/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import { itWebGPU, visitTestPage } from './lane';

// Minimal WebGPU typings. lib.dom of TypeScript ~5.0 does not ship WebGPU declarations, so
// structural types are used; real WebGPU objects satisfy them.
interface GPUAdapterLike {
  requestDevice: () => Promise<GPUDeviceLike>;
}

interface GPUDeviceLike {
  createTexture: (descriptor: object) => any;
  queue: object;
}

interface NavigatorWithGPU extends Navigator {
  gpu?: { requestAdapter: () => Promise<GPUAdapterLike | null> };
}

interface GPUTextureUsageConstants {
  RENDER_ATTACHMENT: number;
  COPY_SRC: number;
  TEXTURE_BINDING: number;
  COPY_DST: number;
}

describe('PAGWebGPU', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let device: GPUDeviceLike;
  let textureUsage: GPUTextureUsageConstants;

  beforeEach(() => {
    visitTestPage();
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      const nav = window.navigator as NavigatorWithGPU;
      expect(nav.gpu, 'WebGPU is not available in this browser').to.not.be.undefined;
      const adapter = await nav.gpu!.requestAdapter();
      expect(adapter, 'requestAdapter() returned null').to.not.be.null;
      device = await adapter!.requestDevice();
      // The GPUTexture must come from the same device that drives PAG rendering, so the device is
      // passed into PAGInit instead of letting libpag request its own one.
      PAG = await window.libpag.PAGInit({ webGPUDevice: device });
      PAGTypes = window.libpag.types;
      textureUsage = (window as unknown as { GPUTextureUsage: GPUTextureUsageConstants }).GPUTextureUsage;
    });
  });

  const makeRenderTargetTexture = () =>
    device.createTexture({
      size: { width: 300, height: 300 },
      format: 'rgba8unorm',
      usage: textureUsage.RENDER_ATTACHMENT | textureUsage.COPY_SRC,
    });

  itWebGPU('PAGInit with a host GPUDevice', () => {
    expect(PAG.PAGPlayer.create().wasmIns).to.be.a('Object');
  });

  itWebGPU('Make PAGSurface from a GPUTexture', () => {
    const pagSurface = PAG.PAGSurface.fromTexture(makeRenderTargetTexture(), 300, 300, true);
    expect(pagSurface.wasmIns).to.be.a('Object');
  });

  itWebGPU('Read pixels asynchronously', async () => {
    const pagSurface = PAG.PAGSurface.fromTexture(makeRenderTargetTexture(), 300, 300, true);
    const pagPlayer = PAG.PAGPlayer.create();
    const buffer = await global.fetch('/demo/assets/AudioMarker.pag').then((res) => res.arrayBuffer());
    const pagFile = await PAG.PAGFile.load(buffer);
    pagPlayer.setSurface(pagSurface);
    pagPlayer.setComposition(pagFile);
    pagPlayer.setProgress(0.5);
    pagPlayer.setScaleMode(PAGTypes.PAGScaleMode.Stretch);
    await pagPlayer.flush();
    const pixels = await pagSurface.readPixelsAsync(
      PAGTypes.ColorType.RGBA_8888,
      PAGTypes.AlphaType.Unpremultiplied,
      30000,
    );
    expect(pixels).to.not.be.null;
    expect(pixels!.byteLength).to.be.eq(360000);
  });

  itWebGPU('Rejects a GPUTexture without COPY_SRC', () => {
    const texture = device.createTexture({
      size: { width: 300, height: 300 },
      format: 'rgba8unorm',
      usage: textureUsage.RENDER_ATTACHMENT,
    });
    expect(() => PAG.PAGSurface.fromTexture(texture, 300, 300, true)).to.throw();
  });

  itWebGPU('Synchronous readPixels returns null', () => {
    const pagSurface = PAG.PAGSurface.fromTexture(makeRenderTargetTexture(), 300, 300, true);
    expect(pagSurface.readPixels(PAGTypes.ColorType.RGBA_8888, PAGTypes.AlphaType.Unpremultiplied)).to.be.null;
  });

  itWebGPU('fromRenderTarget is rejected', () => {
    expect(() => PAG.PAGSurface.fromRenderTarget(0, 300, 300, true)).to.throw();
  });

  itWebGPU('Make PAGSurface from a canvas object', () => {
    const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
    const pagSurface = PAG.PAGSurface.fromCanvas(canvas);
    expect(pagSurface.wasmIns).to.be.a('Object');
  });

  itWebGPU('Make PAGImage from a GPUTexture', () => {
    const texture = device.createTexture({
      size: { width: 300, height: 300 },
      format: 'rgba8unorm',
      usage: textureUsage.TEXTURE_BINDING | textureUsage.COPY_DST,
    });
    const pagImage = PAG.PAGImage.fromTexture(texture, 300, 300, true);
    expect(pagImage.wasmIns).to.be.a('Object');
  });
});
