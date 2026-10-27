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

  itWebGPU('PAGView renders through the canvas2D composite path', async () => {
    const buffer = await global.fetch('/demo/assets/test.pag').then((res) => res.arrayBuffer());
    const pagFile = await PAG.PAGFile.load(buffer);
    const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
    const pagView = await PAG.PAGView.init(pagFile, canvas, { useCanvas2D: true });
    expect(pagView).to.not.be.undefined;
    const snapshot = await pagView!.makeSnapshot();
    const probe = global.document.createElement('canvas');
    probe.width = snapshot.width;
    probe.height = snapshot.height;
    const ctx = probe.getContext('2d');
    ctx!.drawImage(snapshot, 0, 0);
    const data = ctx!.getImageData(0, 0, probe.width, probe.height).data;
    let hasVisiblePixel = false;
    for (let i = 3; i < data.length; i += 4) {
      if (data[i] > 0) {
        hasVisiblePixel = true;
        break;
      }
    }
    expect(hasVisiblePixel, 'the composite must draw visible pixels').to.be.true;
    pagView!.destroy();
  });

  itWebGPU('readPixelsAsync returns null on canvas-backed surfaces', async () => {
    const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
    const pagSurface = PAG.PAGSurface.fromCanvas(canvas);
    const pixels = await pagSurface.readPixelsAsync(
      PAGTypes.ColorType.RGBA_8888,
      PAGTypes.AlphaType.Unpremultiplied,
    );
    expect(pixels).to.be.null;
  });

  itWebGPU('PAGView assigns an id to a canvas without one', async () => {
    const canvas = global.document.createElement('canvas');
    canvas.width = 300;
    canvas.height = 300;
    global.document.body.appendChild(canvas);
    const buffer = await global.fetch('/demo/assets/test.pag').then((res) => res.arrayBuffer());
    const pagFile = await PAG.PAGFile.load(buffer);
    const pagView = await PAG.PAGView.init(pagFile, canvas);
    expect(canvas.id.startsWith('pag-webgpu-canvas-')).to.be.true;
    pagView!.destroy();
    global.document.body.removeChild(canvas);
  });

  itWebGPU('PAGSurface destroy releases the WebGPU texture handle', () => {
    const pagSurface = PAG.PAGSurface.fromTexture(makeRenderTargetTexture(), 300, 300, true);
    pagSurface.destroy();
    expect(pagSurface.isDestroyed).to.be.true;
    // The handle is private; reading it directly is the only way to verify that destroy()
    // released the registered GPUTexture instead of leaking it until garbage collection.
    const handle = (pagSurface as unknown as { webGPUTextureHandle: number }).webGPUTextureHandle;
    expect(handle, 'destroy() must clear the registered texture handle').to.be.eq(0);
    const recreated = PAG.PAGSurface.fromTexture(makeRenderTargetTexture(), 300, 300, true);
    expect(recreated.wasmIns).to.be.a('Object');
    recreated.destroy();
  });
});
