/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGImage } from '../../src/pag-image';
import type { PAGView } from '../../src/pag-view';

describe('PAGImage', () => {
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

  let pagImage: PAGImage;
  it('Make from file', async () => {
    const imageBlob = await global.fetch('/demo/assets/cat.png').then((res) => res.blob());
    pagImage = await PAG.PAGImage.fromFile(new File([imageBlob], 'cat.png'));
    expect(pagImage.wasmIns).to.be.a('object');
  });

  it('Make from source', async () => {
    const image = await new Promise<HTMLImageElement>((resolve) => {
      const image = new window.Image();
      image.src = '/demo/assets/cat.png';
      image.onload = () => {
        resolve(image);
      };
    });
    pagImage = await PAG.PAGImage.fromSource(image);
    expect(pagImage.wasmIns).to.be.a('object');
  });

  it('Make from pixels', async () => {
    const imageBlob = await fetch('/demo/assets/cat.png').then((res) => res.blob());
    const imageBitmap = await createImageBitmap(imageBlob);
    const canvas = document.createElement('canvas');
    canvas.width = imageBitmap.width;
    canvas.height = imageBitmap.height;
    const ctx = canvas.getContext('2d');

    ctx.drawImage(imageBitmap, 0, 0);

    const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);

    const pixels = new Uint8Array(imageData.data.buffer);

    const pagImage = PAG.PAGImage.fromPixels(
      pixels,
      canvas.width,
      canvas.height,
      PAGTypes.ColorType.RGBA_8888,
      PAGTypes.AlphaType.Opaque,
    );

    expect(pagImage.wasmIns).to.be.a('object');
  });

  it('Make from Texture', async () => {
    const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
    const gl = canvas.getContext('webgl2', {
      depth: false,
      stencil: false,
      antialias: false,
    });
    expect(!!gl).to.be.eq(true);
    const pagGLCtx = PAG.BackendContext.from(gl);
    expect(pagGLCtx.makeCurrent()).to.be.eq(true);
    const pagImageTexture = PAG.PAGImage.fromTexture(1, canvas.width, canvas.height, true);
    pagGLCtx.clearCurrent();
    expect(pagImageTexture.wasmIns).to.be.a('object');
  });

  it('Get size', () => {
    expect(pagImage.width()).to.be.eq(300);
    expect(pagImage.height()).to.be.eq(300);
  });

  it('Get/Set scaleMode', () => {
    expect(pagImage.scaleMode()).to.be.eq(PAGTypes.PAGScaleMode.LetterBox);
    pagImage.setScaleMode(PAGTypes.PAGScaleMode.Zoom);
    expect(pagImage.scaleMode()).to.be.eq(PAGTypes.PAGScaleMode.Zoom);
  });

  it('matrix', () => {
    let matrix = pagImage.matrix();
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 0, 0]);
    matrix.set(PAGTypes.MatrixIndex.a, 2);
    pagImage.setMatrix(matrix);
    matrix = pagImage.matrix();
    expect(matrix.a).to.be.eq(2);
  });
});
