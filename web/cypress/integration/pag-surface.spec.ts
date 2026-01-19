/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';

describe('PAGSurface', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  beforeEach(() => {
    cy.visit('/cypress/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
    });
  });

  it('Make PAGSurface from canvas', () => {
    const pagSurface = PAG.PAGSurface.fromCanvas('#pag');
    expect(pagSurface.wasmIns).to.be.a('Object');
  });

  it('Make PAGSurface from Texture', () => {
    const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
    const gl = canvas.getContext('webgl2', {
      depth: false,
      stencil: false,
      antialias: false,
    });
    expect(!!gl).to.be.eq(true);
    const pagGLCtx = PAG.BackendContext.from(gl);
    expect(pagGLCtx.makeCurrent()).to.be.eq(true);
    const pagSurface = PAG.PAGSurface.fromTexture(0, canvas.width, canvas.height, true);
    pagGLCtx.clearCurrent();
    expect(!!pagSurface.wasmIns).to.be.eq(true);
  });

  const makePAGSurfaceFromRenderTarget = () => {
    const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
    const gl = canvas.getContext('webgl2', {
      depth: false,
      stencil: false,
      antialias: false,
    });
    expect(!!gl).to.be.eq(true);
    const pagGLCtx = PAG.BackendContext.from(gl);
    expect(pagGLCtx.makeCurrent()).to.be.eq(true);
    const pagSurface = PAG.PAGSurface.fromRenderTarget(0, canvas.width, canvas.height, true);
    pagGLCtx.clearCurrent();
    expect(!!pagSurface.wasmIns).to.be.eq(true);
    return pagSurface;
  };

  it('Make PAGSurface from RenderTarget', () => {
    makePAGSurfaceFromRenderTarget();
  });

  it('Get size', () => {
    const pagSurface = makePAGSurfaceFromRenderTarget();
    expect(pagSurface.width()).to.be.eq(300);
    expect(pagSurface.height()).to.be.eq(300);
  });

  it('Read pixels', async () => {
    const pagSurface = makePAGSurfaceFromRenderTarget();
    const pagPlayer = PAG.PAGPlayer.create();
    const buffer = await global.fetch('/demo/assets/AudioMarker.pag').then((res) => res.arrayBuffer());
    const pagFile = await PAG.PAGFile.load(buffer);
    expect(pagFile.wasmIns).to.be.a('Object');
    pagPlayer.setSurface(pagSurface);
    pagPlayer.setComposition(pagFile);
    pagPlayer.setProgress(0.5);
    pagPlayer.setScaleMode(PAGTypes.PAGScaleMode.Stretch);
    await pagPlayer.flush();
    expect(pagSurface.readPixels(PAGTypes.ColorType.RGBA_8888, PAGTypes.AlphaType.Unpremultiplied).byteLength).to.be.eq(
      360000,
    );
  });

  it('Clear All', () => {
    const pagSurface = makePAGSurfaceFromRenderTarget();
    pagSurface.clearAll();
  });

  it('Free Cache', () => {
    const pagSurface = makePAGSurfaceFromRenderTarget();
    pagSurface.freeCache();
  });
});
