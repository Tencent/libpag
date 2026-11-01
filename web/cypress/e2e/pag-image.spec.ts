/* eslint prefer-arrow-callback: ["error", { "allowNamedFunctions": true }] */
/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach afterEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGImage } from '../../src/pag-image';
import type { BackendContext } from '../../src/core/backend-context';
import type { Matrix } from '../../src/core/matrix';
import { itGL, visitTestPage } from './lane';

describe('PAGImage', function imageTests() {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  let pagImage: PAGImage;
  let ownedImages: PAGImage[];
  let ownedMatrices: Matrix[];
  let imageBitmap: ImageBitmap | undefined;
  let gl: WebGL2RenderingContext | null;
  let pagGLContext: BackendContext | undefined;
  let texture: WebGLTexture | null;
  let textureHandle: number;

  function ownImage(image: PAGImage): PAGImage {
    ownedImages.push(image);
    return image;
  }

  function ownMatrix(matrix: Matrix): Matrix {
    ownedMatrices.push(matrix);
    return matrix;
  }

  beforeEach(function setupImage() {
    ownedImages = [];
    ownedMatrices = [];
    imageBitmap = undefined;
    gl = null;
    pagGLContext = undefined;
    texture = null;
    textureHandle = 0;
    visitTestPage();
    cy.window().then(async function loadImage(window: Cypress.AUTWindow & { libpag: typeof Libpag }) {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
      const response = await window.fetch('/demo/assets/cat.png');
      const imageBlob = await response.blob();
      pagImage = ownImage(await PAG.PAGImage.fromFile(new window.File([imageBlob], 'cat.png')));
    });
  });

  afterEach(function destroyImage() {
    for (const matrix of ownedMatrices.reverse()) {
      matrix.destroy();
    }
    for (const image of ownedImages.reverse()) {
      image.destroy();
    }
    imageBitmap?.close();
    if (textureHandle) {
      pagGLContext.unregisterTexture(textureHandle);
    }
    if (gl && texture) {
      gl.bindTexture(gl.TEXTURE_2D, null);
      gl.deleteTexture(texture);
    }
    pagGLContext?.clearCurrent();
    pagGLContext?.destroy();
  });

  it('Make from file', function makeFromFile() {
    expect(pagImage.wasmIns).to.be.a('object');
  });

  it('Make from source', async function makeFromSource() {
    const image = new global.Image();
    image.src = '/demo/assets/cat.png';
    await image.decode();
    const sourceImage = ownImage(PAG.PAGImage.fromSource(image));
    expect(sourceImage.wasmIns).to.be.a('object');
  });

  it('Make from pixels', async function makeFromPixels() {
    const response = await global.fetch('/demo/assets/cat.png');
    imageBitmap = await global.createImageBitmap(await response.blob());
    const canvas = global.document.createElement('canvas');
    canvas.width = imageBitmap.width;
    canvas.height = imageBitmap.height;
    const context = canvas.getContext('2d');
    expect(context).not.to.be.eq(null);
    context.drawImage(imageBitmap, 0, 0);
    const imageData = context.getImageData(0, 0, canvas.width, canvas.height);
    const pixels = new global.Uint8Array(imageData.data.buffer);
    const pixelImage = ownImage(PAG.PAGImage.fromPixels(
      pixels,
      canvas.width,
      canvas.height,
      PAGTypes.ColorType.RGBA_8888,
      PAGTypes.AlphaType.Opaque,
    ));
    expect(pixelImage.wasmIns).to.be.a('object');
  });

  itGL('Make from Texture', function makeFromTexture() {
    const canvas = global.document.getElementById('pag') as HTMLCanvasElement;
    gl = canvas.getContext('webgl2', {
      depth: false,
      stencil: false,
      antialias: false,
    });
    expect(gl).not.to.be.eq(null);
    pagGLContext = PAG.BackendContext.from(gl);
    expect(pagGLContext.makeCurrent()).to.be.eq(true);
    texture = gl.createTexture();
    expect(texture).not.to.be.eq(null);
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, canvas.width, canvas.height, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
    expect(gl.getError()).to.be.eq(gl.NO_ERROR);
    textureHandle = pagGLContext.registerTexture(texture);
    expect(pagGLContext.getTexture(textureHandle)).to.be.eq(texture);
    const textureImage = ownImage(PAG.PAGImage.fromTexture(textureHandle, canvas.width, canvas.height, true));
    expect(textureImage.wasmIns).to.be.a('object');
    expect(textureImage.width()).to.be.eq(canvas.width);
    expect(textureImage.height()).to.be.eq(canvas.height);
  });

  it('Get size', function getSize() {
    expect(pagImage.width()).to.be.eq(300);
    expect(pagImage.height()).to.be.eq(300);
  });

  it('Get/Set scaleMode', function setScaleMode() {
    expect(pagImage.scaleMode()).to.be.eq(PAGTypes.PAGScaleMode.LetterBox);
    pagImage.setScaleMode(PAGTypes.PAGScaleMode.Zoom);
    expect(pagImage.scaleMode()).to.be.eq(PAGTypes.PAGScaleMode.Zoom);
  });

  it('matrix', function setMatrix() {
    const matrix = ownMatrix(pagImage.matrix());
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 0, 0]);
    matrix.set(PAGTypes.MatrixIndex.a, 2);
    pagImage.setMatrix(matrix);
    expect(ownMatrix(pagImage.matrix()).a).to.be.eq(2);
  });
});
