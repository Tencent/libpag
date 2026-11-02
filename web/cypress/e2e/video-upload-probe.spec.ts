/* eslint-disable max-nested-callbacks */
/* global describe expect cy beforeEach afterEach */
// Temporary probe for verifying the retry-every-frame WebGPU video upload strategy on software
// adapters. It proves that copyExternalImageToTexture fails synchronously (never through the
// async error scope), that every frame retries the fast path and falls back, and measures the
// cost of a rejected import versus the CPU readback. Delete together with the tgfx
// instrumentation (module.videoUploadStats).
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGSurface } from '../../src/pag-surface';
import type { WebGPUTextureLike } from '../../src/core/webgpu-texture';
import { itWebGPU, visitTestPage } from './lane';

interface GPUTexture extends WebGPUTextureLike {
  destroy: () => void;
}

interface GPUDevice {
  createTexture: (descriptor: object) => GPUTexture;
  queue: {
    writeTexture: (destination: object, data: Uint8Array, layout: object, size: object) => void;
    onSubmittedWorkDone: () => Promise<void>;
  };
  pushErrorScope: (filter: string) => void;
  popErrorScope: () => Promise<{ message: string } | null>;
  addEventListener: (type: string, listener: (event: { error: { message: string } }) => void) => void;
  lost: Promise<{ reason: string; message: string }>;
  destroy: () => void;
}

interface NavigatorWithGPU extends Navigator {
  gpu?: { requestAdapter: () => Promise<{ requestDevice: () => Promise<GPUDevice> } | null> };
}

interface GPUTextureUsageConstants {
  RENDER_ATTACHMENT: number;
  COPY_SRC: number;
}

interface Disposable {
  isDestroyed?: boolean;
  destroy: () => void;
}

interface VideoUploadStats {
  calls: number;
  notReady: number;
  noDeviceOrTexture: number;
  copyAttempts: number;
  copyThrew: number;
  lastErrorName: string;
  asyncErrors: number;
  fallbackRuns: number;
  fallbackThrew: number;
  rejectedImportAvgUs: number;
  readbackAvgMs: number;
}

describe('VideoUploadProbe', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let device: GPUDevice | null;
  let textureUsage: GPUTextureUsageConstants;
  let resources: Disposable[];
  let textures: GPUTexture[];

  function Own<T extends Disposable>(resource: T): T {
    resources.push(resource);
    return resource;
  }

  beforeEach(() => {
    resources = [];
    textures = [];
    device = null;
    visitTestPage();
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      const nav = window.navigator as NavigatorWithGPU;
      expect(nav.gpu, 'WebGPU must be available').to.not.equal(undefined);
      const adapter = await nav.gpu!.requestAdapter();
      expect(adapter, 'a GPU adapter must be available').to.not.equal(null);
      device = await adapter!.requestDevice();
      PAG = await window.libpag.PAGInit({ webGPUDevice: device });
      expect(PAG._IsWebGPUBackend(), 'actual WASM backend').to.equal(true);
      PAGTypes = window.libpag.types;
      textureUsage = (window as unknown as { GPUTextureUsage: GPUTextureUsageConstants }).GPUTextureUsage;
    });
  });

  afterEach(() => {
    for (const resource of resources.reverse()) {
      if (!resource.isDestroyed) resource.destroy();
    }
    for (const texture of textures) texture.destroy();
    device?.destroy();
  });

  function MakeSurface(): PAGSurface {
    const texture = device!.createTexture({ size: { width: 300, height: 300 }, format: 'rgba8unorm',
      usage: textureUsage.RENDER_ATTACHMENT | textureUsage.COPY_SRC });
    textures.push(texture);
    return Own(PAG.PAGSurface.fromTexture(texture, 300, 300, false));
  }

  itWebGPU('Every frame retries the fast path and falls back after a synchronous throw', async () => {
    const buffer = await global.fetch('/demo/assets/AudioMarker.pag').then((res) => res.arrayBuffer());
    const pagFile = Own(await PAG.PAGFile.load(buffer));
    expect(pagFile.numVideos(), 'test asset must contain a video').to.be.greaterThan(0);
    const read = (surface: PAGSurface) =>
      surface.readPixelsAsync(PAGTypes.ColorType.RGBA_8888, PAGTypes.AlphaType.Unpremultiplied);

    const player = Own(PAG.PAGPlayer.create());
    const surface = MakeSurface();
    player.setSurface(surface);
    player.setComposition(pagFile);
    player.setScaleMode(PAGTypes.PAGScaleMode.None);
    // Step through several frames so the upload path runs repeatedly on every frame.
    for (const progress of [0.1, 0.25, 0.4, 0.55, 0.7, 0.85]) {
      player.setProgress(progress);
      await player.prepare();
      await player.flush();
      await new Promise((resolve) => { setTimeout(resolve, 150); });
    }
    const withVideo = await read(surface);

    // A fresh player on a fresh surface avoids the frame cache, so the only difference
    // between the two reads is the hidden video layer. Any silent upload failure (an async
    // validation error or a no-op copy) leaves the video texture empty and the reads equal.
    const referencePlayer = Own(PAG.PAGPlayer.create());
    const referenceSurface = MakeSurface();
    referencePlayer.setSurface(referenceSurface);
    referencePlayer.setComposition(pagFile);
    referencePlayer.setScaleMode(PAGTypes.PAGScaleMode.None);
    referencePlayer.setVideoEnabled(false);
    referencePlayer.setProgress(0.85);
    await referencePlayer.prepare();
    await referencePlayer.flush();
    const withoutVideo = await read(referenceSurface);

    expect(withVideo).to.not.equal(null);
    expect(withoutVideo).to.not.equal(null);
    let different = 0;
    for (let i = 0; i < withVideo!.length; i++) {
      if (Math.abs(withVideo![i] - withoutVideo![i]) > 1) different++;
    }
    expect(different, 'video pixels must reach the texture').to.be.greaterThan(100);

    const stats = (PAG as unknown as { videoUploadStats?: VideoUploadStats }).videoUploadStats;
    expect(stats, 'tgfx instrumentation (module.videoUploadStats)').to.not.equal(undefined);
    cy.task('print', `videoUploadStats: ${JSON.stringify(stats)}`);
    expect(stats!.calls, 'upload entry').to.be.greaterThan(0);
    expect(stats!.copyAttempts, 'every frame retries the fast path').to.equal(stats!.calls);
    expect(stats!.asyncErrors, 'failures must never arrive through the async error scope').to.equal(0);
    expect(stats!.fallbackThrew, 'writeTexture failures').to.equal(0);
    if (stats!.copyThrew > 0) {
      expect(stats!.copyThrew, 'software adapters must fail synchronously').to.equal(stats!.copyAttempts);
      expect(stats!.lastErrorName).to.equal('OperationError');
      expect(stats!.fallbackRuns, 'every rejected frame falls back').to.equal(stats!.calls);
      expect(stats!.rejectedImportAvgUs, 'the rejected-import benchmark ran').to.be.greaterThan(0);
      expect(stats!.readbackAvgMs, 'the readback timing ran').to.be.greaterThan(0);
      expect(stats!.rejectedImportAvgUs, 'a rejected import must stay far cheaper than the readback')
        .to.be.lessThan(stats!.readbackAvgMs * 100);
    } else {
      expect(stats!.fallbackRuns, 'hardware adapters keep the zero-copy path').to.equal(0);
    }
  });
});
