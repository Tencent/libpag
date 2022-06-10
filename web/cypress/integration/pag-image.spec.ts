/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import type { PAGImage } from '../../src/pag-image';

describe('PAGImage', () => {
  let PAG: PAG;
  let global: Cypress.AUTWindow;
  let PAGTypes: typeof Libpag.types;
  beforeEach(() => {
    cy.visit('/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      global = window;
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
    });
  });

  let pagImage: PAGImage;
  it('Make from file', async () => {
    const imageBlob = await global.fetch('http://127.0.0.1:8080/demo/assets/cat.png').then((res) => res.blob());
    pagImage = await PAG.PAGImage.fromFile(new File([imageBlob], 'cat.png'));
    expect(pagImage.wasmIns).to.be.a('object');
  });

  it('Make from source', async () => {
    const image = await new Promise<HTMLImageElement>((resolve) => {
      const image = new window.Image();
      image.src = 'http://127.0.0.1:8080/demo/assets/cat.png';
      image.onload = () => {
        resolve(image);
      };
    });
    pagImage = await PAG.PAGImage.fromSource(image);
    expect(pagImage.wasmIns).to.be.a('object');
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
