/* eslint-disable max-nested-callbacks */
/* global describe expect cy Cypress beforeEach afterEach */
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
  TEXTURE_BINDING: number;
  COPY_DST: number;
}

interface Disposable {
  isDestroyed?: boolean;
  destroy: () => void;
}

describe('PAGWebGPU', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let device: GPUDevice | null;
  let textureUsage: GPUTextureUsageConstants;
  let resources: Disposable[];
  let textures: GPUTexture[];
  let gpuErrors: string[];
  let scopeCount: number;

  function Own<T extends Disposable>(resource: T): T {
    resources.push(resource);
    return resource;
  }

  beforeEach(() => {
    resources = [];
    textures = [];
    gpuErrors = [];
    scopeCount = 0;
    device = null;
    visitTestPage();
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      const nav = window.navigator as NavigatorWithGPU;
      expect(nav.gpu, 'WebGPU must be available').to.not.equal(undefined);
      const adapter = await nav.gpu!.requestAdapter();
      expect(adapter, 'a GPU adapter must be available').to.not.equal(null);
      device = await adapter!.requestDevice();
      const errors = gpuErrors;
      device.addEventListener('uncapturederror', (event) => { errors.push(event.error.message); });
      device.lost.then((info) => {
        if (info.reason !== 'destroyed') errors.push(`Device lost: ${info.message}`);
      });
      for (const filter of ['validation', 'internal', 'out-of-memory']) {
        device.pushErrorScope(filter);
        scopeCount++;
      }
      PAG = await window.libpag.PAGInit({ webGPUDevice: device });
      expect(PAG._IsWebGPUBackend(), 'actual WASM backend').to.equal(true);
      PAGTypes = window.libpag.types;
      textureUsage = (window as unknown as { GPUTextureUsage: GPUTextureUsageConstants }).GPUTextureUsage;
    });
  });

  afterEach(async () => {
    try {
      for (const resource of resources.reverse()) {
        if (!resource.isDestroyed) resource.destroy();
      }
      if (device) {
        // Device-level waits reject with DOMExceptions whose message is getter-only, which
        // Cypress cannot rethrow without crashing the whole suite, so normalize every
        // failure into a plain string before the final assertion.
        try {
          await device.queue.onSubmittedWorkDone();
        } catch (waitError) {
          gpuErrors.push(`Flush wait failed: ${String(waitError)}`);
        }
        for (const texture of textures) texture.destroy();
        while (scopeCount > 0) {
          scopeCount--;
          try {
            const error = await device.popErrorScope();
            if (error) gpuErrors.push(error.message);
          } catch (scopeError) {
            gpuErrors.push(`Error scope collection failed: ${String(scopeError)}`);
          }
        }
      }
      expect(gpuErrors, 'unexpected GPU errors').to.deep.equal([]);
    } finally {
      device?.destroy();
    }
  });

  function MakeTexture(width = 65, height = 3, sampled = false): GPUTexture {
    const usage = sampled
      ? textureUsage.TEXTURE_BINDING | textureUsage.COPY_DST
      : textureUsage.RENDER_ATTACHMENT | textureUsage.COPY_SRC;
    const texture = device!.createTexture({ size: { width, height }, format: 'rgba8unorm', usage });
    textures.push(texture);
    return texture;
  }

  function MakePattern() {
    const composition = Own(PAG.PAGComposition.make(65, 3));
    const colors = [{ red: 255, green: 0, blue: 0 }, { red: 0, green: 255, blue: 0 },
      { red: 0, green: 0, blue: 255 }];
    const layers = colors.map((color, y) => {
      const layer = Own(PAG.PAGSolidLayer.make(1000000, y === 1 ? 32 : 65, 1, color, y === 1 ? 128 : 255));
      const matrix = Own(PAG.Matrix.makeTrans(0, y));
      layer.setMatrix(matrix);
      composition.addLayer(layer);
      return layer;
    });
    return { composition, layers };
  }

  function ExpectedPattern(top: number[] = [255, 0, 0, 255]): Uint8Array {
    const pixels = new Uint8Array(65 * 3 * 4);
    for (let y = 0; y < 3; y++) {
      for (let x = 0; x < 65; x++) {
        let color = top;
        if (y === 1) color = x < 32 ? [0, 255, 0, 128] : [0, 0, 0, 0];
        if (y === 2) color = [0, 0, 255, 255];
        pixels.set(color, (y * 65 + x) * 4);
      }
    }
    return pixels;
  }

  function ExpectPixels(actual: Uint8Array | Uint8ClampedArray | null, expected: Uint8Array): void {
    expect(actual, 'pixel data').to.not.equal(null);
    expect(actual!.length, 'pixel byte count').to.equal(expected.length);
    let mismatches = 0;
    for (let i = 0; i < expected.length; i++) {
      if (Math.abs(actual![i] - expected[i]) > 1) mismatches++;
    }
    expect(mismatches, 'RGBA values, alpha, origin and row layout').to.equal(0);
  }

  async function Read(surface: PAGSurface): Promise<Uint8Array | null> {
    return surface.readPixelsAsync(PAGTypes.ColorType.RGBA_8888, PAGTypes.AlphaType.Unpremultiplied);
  }

  function Attach(surface: PAGSurface, composition: ReturnType<typeof MakePattern>['composition']) {
    const player = Own(PAG.PAGPlayer.create());
    player.setSurface(surface);
    player.setComposition(composition);
    player.setScaleMode(PAGTypes.PAGScaleMode.None);
    return player;
  }

  async function ExpectSnapshot(view: InstanceType<PAG['PAGView']>, expected: Uint8Array): Promise<void> {
    const snapshot = await view.makeSnapshot();
    try {
      expect([snapshot.width, snapshot.height]).to.deep.equal([65, 3]);
      const probe = global.document.createElement('canvas');
      probe.width = 65;
      probe.height = 3;
      const context = probe.getContext('2d')!;
      context.drawImage(snapshot, 0, 0);
      ExpectPixels(context.getImageData(0, 0, 65, 3).data, expected);
    } finally {
      snapshot.close();
    }
  }

  itWebGPU('PAGInit uses the host GPUDevice and the WebGPU backend', () => {
    expect(PAG.preinitializedWebGPUDevice).to.equal(device);
    expect(Own(PAG.PAGPlayer.create()).wasmIns).to.be.an('object');
  });

  itWebGPU('Make PAGSurface from a GPUTexture', () => {
    const surface = Own(PAG.PAGSurface.fromTexture(MakeTexture(), 65, 3, false));
    expect([surface.width(), surface.height()]).to.deep.equal([65, 3]);
  });

  itWebGPU('Reads exact pixels across padded rows and preserves earlier readbacks', async () => {
    const surface = Own(PAG.PAGSurface.fromTexture(MakeTexture(), 65, 3, false));
    const pattern = MakePattern();
    const player = Attach(surface, pattern.composition);
    await player.flush();
    const first = await Read(surface);
    ExpectPixels(first, ExpectedPattern());
    pattern.layers[0].setSolidColor({ red: 255, green: 255, blue: 0 });
    await player.flush();
    ExpectPixels(await Read(surface), ExpectedPattern([255, 255, 0, 255]));
    ExpectPixels(first, ExpectedPattern());
    const alpha = await surface.readPixelsAsync(PAGTypes.ColorType.ALPHA_8, PAGTypes.AlphaType.Unpremultiplied);
    expect(alpha).to.not.equal(null);
    expect(Array.from(alpha!)).to.deep.equal(Array.from(ExpectedPattern()).filter((_, i) => { return i % 4 === 3; }));
    expect(surface.clearAll()).to.equal(true);
    ExpectPixels(await Read(surface), new Uint8Array(65 * 3 * 4));
  });

  itWebGPU('Rejects a GPUTexture without COPY_SRC', () => {
    const texture = device!.createTexture({ size: { width: 65, height: 3 }, format: 'rgba8unorm',
      usage: textureUsage.RENDER_ATTACHMENT });
    textures.push(texture);
    expect(() => { PAG.PAGSurface.fromTexture(texture, 65, 3, false); }).to.throw('COPY_SRC');
  });

  itWebGPU('Synchronous readPixels returns null', () => {
    const surface = Own(PAG.PAGSurface.fromTexture(MakeTexture(), 65, 3, false));
    expect(surface.readPixels(PAGTypes.ColorType.RGBA_8888, PAGTypes.AlphaType.Unpremultiplied)).to.equal(null);
  });

  itWebGPU('fromRenderTarget is rejected', () => {
    expect(() => { PAG.PAGSurface.fromRenderTarget(0, 65, 3, false); }).to.throw('not supported on the WebGPU backend');
  });

  itWebGPU('Make PAGSurface from a canvas object', () => {
    const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
    const surface = Own(PAG.PAGSurface.fromCanvas(canvas));
    expect([surface.width(), surface.height()]).to.deep.equal([canvas.width, canvas.height]);
  });

  itWebGPU('Samples a host GPUTexture with the expected colors and origin', async () => {
    const texture = MakeTexture(65, 3, true);
    const expected = ExpectedPattern();
    for (let x = 0; x < 32; x++) expected[(65 + x) * 4 + 3] = 255;
    device!.queue.writeTexture({ texture }, expected, { bytesPerRow: 65 * 4, rowsPerImage: 3 },
      { width: 65, height: 3 });
    const image = Own(PAG.PAGImage.fromTexture(texture, 65, 3, false));
    const layer = Own(PAG.PAGImageLayer.make(65, 3, 1000000));
    layer.setImage(image);
    const composition = Own(PAG.PAGComposition.make(65, 3));
    composition.addLayer(layer);
    const surface = Own(PAG.PAGSurface.fromTexture(MakeTexture(), 65, 3, false));
    await Attach(surface, composition).flush();
    ExpectPixels(await Read(surface), expected);
  });

  itWebGPU('PAGView renders exact pixels through canvas2D compositing', async () => {
    const canvas = global.document.createElement('canvas');
    canvas.width = 65;
    canvas.height = 3;
    const pattern = MakePattern();
    const view = await PAG.PAGView.init(pattern.composition, canvas, { useCanvas2D: true, useScale: false });
    expect(view).to.not.equal(undefined);
    Own(view!);
    await ExpectSnapshot(view!, ExpectedPattern());
  });

  itWebGPU('readPixelsAsync returns null on canvas-backed surfaces', async () => {
    const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
    const surface = Own(PAG.PAGSurface.fromCanvas(canvas));
    expect(await Read(surface)).to.equal(null);
  });

  itWebGPU('Renders to a detached canvas without an id and reinitializes after destroy', async () => {
    const canvas = global.document.createElement('canvas');
    canvas.width = 65;
    canvas.height = 3;
    const pattern = MakePattern();
    const view = await PAG.PAGView.init(pattern.composition, canvas, { useScale: false });
    expect(view).to.not.equal(undefined);
    Own(view!);
    await ExpectSnapshot(view!, ExpectedPattern());
    view!.destroy();
    const recreated = await PAG.PAGView.init(pattern.composition, canvas, { useScale: false });
    expect(recreated).to.not.equal(undefined);
    Own(recreated!);
    await ExpectSnapshot(recreated!, ExpectedPattern());
  });

  itWebGPU('Resizing and destroying one view does not affect another view', async () => {
    const firstCanvas = global.document.createElement('canvas');
    const secondCanvas = global.document.createElement('canvas');
    firstCanvas.width = 65;
    firstCanvas.height = 3;
    secondCanvas.width = 65;
    secondCanvas.height = 3;
    const firstPattern = MakePattern();
    const secondPattern = MakePattern();
    secondPattern.layers[0].setSolidColor({ red: 255, green: 255, blue: 0 });
    const first = await PAG.PAGView.init(firstPattern.composition, firstCanvas, { useScale: false });
    expect(first).to.not.equal(undefined);
    Own(first!);
    const second = await PAG.PAGView.init(secondPattern.composition, secondCanvas, { useScale: false });
    expect(second).to.not.equal(undefined);
    Own(second!);
    await ExpectSnapshot(first!, ExpectedPattern());
    await ExpectSnapshot(second!, ExpectedPattern([255, 255, 0, 255]));
    firstCanvas.width = 130;
    firstCanvas.height = 6;
    first!.updateSize();
    await first!.flush();
    const resized = await first!.makeSnapshot();
    try {
      expect([resized.width, resized.height]).to.deep.equal([130, 6]);
    } finally {
      resized.close();
    }
    await ExpectSnapshot(second!, ExpectedPattern([255, 255, 0, 255]));
    first!.destroy();
    secondPattern.layers[0].setSolidColor({ red: 255, green: 0, blue: 0 });
    await second!.flush();
    await ExpectSnapshot(second!, ExpectedPattern());
  });

  itWebGPU('Destroy releases the registration but preserves the host texture', async () => {
    const texture = MakeTexture();
    const manager = PAG.WebGPU!.mgrTexture!;
    const register = cy.spy(manager, 'create');
    const release = cy.spy(manager, 'release');
    const surface = Own(PAG.PAGSurface.fromTexture(texture, 65, 3, false));
    expect(register.callCount).to.equal(1);
    const handle = register.firstCall.returnValue;
    const player = Attach(surface, MakePattern().composition);
    await player.flush();
    ExpectPixels(await Read(surface), ExpectedPattern());
    player.destroy();
    surface.destroy();
    expect(surface.isDestroyed).to.equal(true);
    const released = release.getCalls().filter((call) => { return call.args[0] === handle; });
    expect(released.length, 'one registration release').to.equal(1);
    const recreated = Own(PAG.PAGSurface.fromTexture(texture, 65, 3, false));
    await Attach(recreated, MakePattern().composition).flush();
    ExpectPixels(await Read(recreated), ExpectedPattern());
  });
});
