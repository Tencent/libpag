/* eslint-disable max-nested-callbacks  */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import { PAG } from '../../src/types';

describe('Matrix', () => {
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

  it('Make from all', () => {
    const matrix = PAG.Matrix.makeAll(1, 0, 0, 0, 1, 0, 0, 0, 1);
    expect([
      matrix.a,
      matrix.b,
      matrix.c,
      matrix.d,
      matrix.tx,
      matrix.ty,
      matrix.get(PAGTypes.MatrixIndex.pers0),
      matrix.get(PAGTypes.MatrixIndex.pers1),
      matrix.get(PAGTypes.MatrixIndex.pers2),
    ]).to.be.eql([1, 0, 0, 1, 0, 0, 0, 0, 1]);
  });

  it('Make from scale', () => {
    let matrix = PAG.Matrix.makeScale(1, 1);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 0, 0]);
    matrix.destroy();
    matrix = PAG.Matrix.makeScale(0.5, 0.5);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([0.5, 0, 0, 0.5, 0, 0]);
  });

  it('Make from trans', () => {
    let matrix = PAG.Matrix.makeTrans(1, 1);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 1, 1]);
  });

  it('Getter/Setter', () => {
    const matrix = PAG.Matrix.makeAll(1, 0, 0, 0, 1, 0);
    expect(matrix.a).to.be.eq(1);
    matrix.a = 2;
    expect(matrix.a).to.be.eq(2);
  });

  it('Get/Set', () => {
    const matrix = PAG.Matrix.makeAll(1, 0, 0, 0, 1, 0);
    matrix.set(PAGTypes.MatrixIndex.b, 1);
    expect(matrix.get(PAGTypes.MatrixIndex.b)).to.be.eq(1);
  });

  it('Set all', () => {
    const matrix = PAG.Matrix.makeAll(1, 0, 0, 0, 1, 0);
    matrix.setAll(2, 1, 3, 1, 2, 3, 4, 4, 4);
    expect([
      matrix.a,
      matrix.b,
      matrix.c,
      matrix.d,
      matrix.tx,
      matrix.ty,
      matrix.get(PAGTypes.MatrixIndex.pers0),
      matrix.get(PAGTypes.MatrixIndex.pers1),
      matrix.get(PAGTypes.MatrixIndex.pers2),
    ]).to.be.eql([2, 1, 1, 2, 3, 3, 4, 4, 4]);
  });

  it('Set affine', () => {
    const matrix = PAG.Matrix.makeAll(1, 0, 0, 0, 1, 0);
    matrix.setAffine(1, 2, 2, 1, 3, 3);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 2, 2, 1, 3, 3]);
  });

  it('Reset', () => {
    const matrix = PAG.Matrix.makeAll(1, 1, 1, 1, 1, 1);
    matrix.reset();
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 0, 0]);
  });

  it('Set translate', () => {
    const matrix = PAG.Matrix.makeTrans(0, 0);
    expect([matrix.tx, matrix.ty]).to.be.eql([0, 0]);
    matrix.setTranslate(1, 1);
    expect([matrix.tx, matrix.ty]).to.be.eql([1, 1]);
  });

  it('Set scale', () => {
    const matrix = PAG.Matrix.makeScale(0, 0);
    expect([matrix.a, matrix.d]).to.be.eql([0, 0]);
    matrix.setScale(1, 1);
    expect([matrix.a, matrix.d]).to.be.eql([1, 1]);
  });

  it('Set rotate', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    matrix.setRotate(90);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([0, 1, -1, 0, 0, 0]);
  });

  it('Set sin/cos', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    matrix.setSinCos(90, 90);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([90, 90, -90, 90, 0, -0]);
  });

  it('Set skew', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    matrix.setSkew(30, 30);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 30, 30, 1, -0, -0]);
  });

  it('Set concat', () => {
    const matrix = PAG.Matrix.makeAll(1, 2, 3, 1, 2, 3);
    const matrix2 = PAG.Matrix.makeTrans(1, 1);
    matrix.setConcat(matrix, matrix2);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 1, 2, 2, 6, 6]);
  });

  it('Pre translate', () => {
    const matrix = PAG.Matrix.makeTrans(0, 0);
    matrix.preTranslate(2, 2);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 2, 2]);
  });

  it('Pre scale', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    matrix.preScale(2, 2);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([2, 0, 0, 2, 0, 0]);
  });

  it('Pre rotate', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    matrix.preRotate(90);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([0, 1, -1, 0, 0, 0]);
  });

  it('Pre skew', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    matrix.preSkew(2, 2);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 2, 2, 1, 0, 0]);
  });

  it('Pre concat', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    const matrix2 = PAG.Matrix.makeTrans(1, 1);
    matrix.preConcat(matrix2);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 1, 1]);
  });

  it('Pose translate', () => {
    const matrix = PAG.Matrix.makeTrans(0, 0);
    matrix.postTranslate(2, 2);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 2, 2]);
  });

  it('Pose scale', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    matrix.postScale(2, 2);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([2, 0, 0, 2, 0, 0]);
  });

  it('Pose rotate', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    matrix.postRotate(90);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([0, 1, -1, 0, 0, 0]);
  });

  it('Pose skew', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    matrix.postSkew(2, 2);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 2, 2, 1, 0, 0]);
  });

  it('Post concat', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    const matrix2 = PAG.Matrix.makeTrans(1, 1);
    matrix.postConcat(matrix2);
    expect([matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty]).to.be.eql([1, 0, 0, 1, 1, 1]);
  });
});
