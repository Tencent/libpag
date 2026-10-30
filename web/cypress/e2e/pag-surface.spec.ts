/* eslint-disable max-nested-callbacks */
/* global describe it expect cy Cypress beforeEach afterEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGSurface } from '../../src/pag-surface';
import type { BackendContext } from '../../src/core/backend-context';
import { itGL, visitTestPage } from './lane';

interface Disposable {
  destroy: () => void;
}

describe('PAGSurface', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let resources: Disposable[];
  let gl: WebGL2RenderingContext | null;
  let context: BackendContext | null;
  let texture: WebGLTexture | null;
  let framebuffer: WebGLFramebuffer | null;
  let textureHandle: number;
  let framebufferHandle: number;

  function Own<T extends Disposable>(resource: T): T {
    resources.push(resource);
    return resource;
  }

  beforeEach(() => {
    resources = [];
    gl = null;
    context = null;
    texture = null;
    framebuffer = null;
    textureHandle = 0;
    framebufferHandle = 0;
    visitTestPage();
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
    });
  });

  afterEach(() => {
    for (const resource of resources.reverse()) resource.destroy();
    if (context) {
      if (framebufferHandle) context.unregisterRenderTarget(framebufferHandle);
      if (textureHandle) context.unregisterTexture(textureHandle);
      gl!.bindFramebuffer(gl!.FRAMEBUFFER, null);
      gl!.bindTexture(gl!.TEXTURE_2D, null);
      if (framebuffer) gl!.deleteFramebuffer(framebuffer);
      if (texture) gl!.deleteTexture(texture);
      context.clearCurrent();
      context.destroy();
    }
  });

  function MakeSurface(renderTarget: boolean): PAGSurface {
    const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
    gl = canvas.getContext('webgl2', { depth: false, stencil: false, antialias: false });
    expect(gl).to.not.equal(null);
    context = PAG.BackendContext.from(gl!);
    expect(context.makeCurrent()).to.equal(true);
    texture = gl!.createTexture();
    expect(texture).to.not.equal(null);
    gl!.bindTexture(gl!.TEXTURE_2D, texture);
    gl!.texParameteri(gl!.TEXTURE_2D, gl!.TEXTURE_MIN_FILTER, gl!.NEAREST);
    gl!.texParameteri(gl!.TEXTURE_2D, gl!.TEXTURE_MAG_FILTER, gl!.NEAREST);
    gl!.texParameteri(gl!.TEXTURE_2D, gl!.TEXTURE_WRAP_S, gl!.CLAMP_TO_EDGE);
    gl!.texParameteri(gl!.TEXTURE_2D, gl!.TEXTURE_WRAP_T, gl!.CLAMP_TO_EDGE);
    gl!.texImage2D(gl!.TEXTURE_2D, 0, gl!.RGBA, 65, 3, 0, gl!.RGBA, gl!.UNSIGNED_BYTE, null);
    if (renderTarget) {
      framebuffer = gl!.createFramebuffer();
      expect(framebuffer).to.not.equal(null);
      gl!.bindFramebuffer(gl!.FRAMEBUFFER, framebuffer);
      gl!.framebufferTexture2D(gl!.FRAMEBUFFER, gl!.COLOR_ATTACHMENT0, gl!.TEXTURE_2D, texture, 0);
      expect(gl!.checkFramebufferStatus(gl!.FRAMEBUFFER)).to.equal(gl!.FRAMEBUFFER_COMPLETE);
      framebufferHandle = context.registerRenderTarget(framebuffer!);
      return Own(PAG.PAGSurface.fromRenderTarget(framebufferHandle, 65, 3, false));
    }
    textureHandle = context.registerTexture(texture!);
    return Own(PAG.PAGSurface.fromTexture(textureHandle, 65, 3, false));
  }

  function MakePlayer(surface: PAGSurface) {
    const composition = Own(PAG.PAGComposition.make(65, 3));
    const layer = Own(PAG.PAGSolidLayer.make(1000000, 65, 3, { red: 255, green: 0, blue: 0 }, 255));
    composition.addLayer(layer);
    const player = Own(PAG.PAGPlayer.create());
    player.setSurface(surface);
    player.setComposition(composition);
    return { player, layer };
  }

  function Read(surface: PAGSurface): Uint8Array {
    const pixels = surface.readPixels(PAGTypes.ColorType.RGBA_8888, PAGTypes.AlphaType.Unpremultiplied);
    expect(pixels).to.not.equal(null);
    expect(pixels!.length).to.equal(65 * 3 * 4);
    return pixels!;
  }

  function ExpectSolid(pixels: Uint8Array, color: number[]): void {
    let mismatches = 0;
    for (let i = 0; i < pixels.length; i++) {
      if (pixels[i] !== color[i % 4]) mismatches++;
    }
    expect(mismatches, 'every pixel matches the rendered color').to.equal(0);
  }

  it('Make PAGSurface from canvas', () => {
    const surface = Own(PAG.PAGSurface.fromCanvas('#pag'));
    expect(surface.wasmIns).to.be.an('object');
  });

  itGL('Make PAGSurface from Texture', async () => {
    const surface = MakeSurface(false);
    expect([surface.width(), surface.height()]).to.deep.equal([65, 3]);
    await MakePlayer(surface).player.flush();
    ExpectSolid(Read(surface), [255, 0, 0, 255]);
  });

  itGL('Make PAGSurface from RenderTarget', () => {
    expect(MakeSurface(true).wasmIns).to.be.an('object');
  });

  itGL('Get size', () => {
    const surface = MakeSurface(true);
    expect([surface.width(), surface.height()]).to.deep.equal([65, 3]);
  });

  itGL('Read pixels', async () => {
    const surface = MakeSurface(true);
    const { player, layer } = MakePlayer(surface);
    await player.flush();
    const first = Read(surface);
    ExpectSolid(first, [255, 0, 0, 255]);
    layer.setSolidColor({ red: 0, green: 0, blue: 255 });
    await player.flush();
    ExpectSolid(Read(surface), [0, 0, 255, 255]);
    ExpectSolid(first, [255, 0, 0, 255]);
  });

  itGL('Clear All', async () => {
    const surface = MakeSurface(true);
    await MakePlayer(surface).player.flush();
    ExpectSolid(Read(surface), [255, 0, 0, 255]);
    expect(surface.clearAll()).to.equal(true);
    ExpectSolid(Read(surface), [0, 0, 0, 0]);
  });

  itGL('Free Cache', async () => {
    const surface = MakeSurface(true);
    const { player } = MakePlayer(surface);
    await player.flush();
    surface.freeCache();
    await player.flush();
    ExpectSolid(Read(surface), [255, 0, 0, 255]);
  });
});
