/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress */
import * as Libpag from '../../src/pag';

describe('PAGImage Test', () => {
  it('PAGInit', () => {
    cy.visit('/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      const PAG = await window.libpag.PAGInit();
      const imageBlob = await fetch('http://127.0.0.1:8080/demo/assets/cat.png').then((res) => res.blob());
      const pagImage = await PAG.PAGImage.fromFile(new File([imageBlob], 'cat.png'));
      expect(pagImage.wasmIns).to.be.a('object');
      expect(pagImage.width()).to.be.eq(300);
      expect(pagImage.height()).to.be.eq(300);
      expect(pagImage.scaleMode()).to.be.eq(2);
      pagImage.setScaleMode(window.libpag.types.PAGScaleMode.Zoom);
      expect(pagImage.scaleMode()).to.be.eq(3);
      let matrix = pagImage.matrix();
      expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 0, 0]);
      matrix.set(window.libpag.types.MatrixIndex.a, 2);
      pagImage.setMatrix(matrix);
      matrix = pagImage.matrix();
      expect(matrix.a).to.be.eq(2);
    });
  });
});
