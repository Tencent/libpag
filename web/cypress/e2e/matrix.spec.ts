/* eslint-disable max-nested-callbacks */
/* eslint-disable @typescript-eslint/no-invalid-this */
/* global describe it expect cy Cypress beforeEach */
import type * as Libpag from '../../src/pag';
import type { PAG } from '../../src/types';
import { visitTestPage } from './lane';

function values(matrix: any): number[] {
  return [matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty];
}

function expectMatrix(matrix: any, expected: number[]): void {
  const actual = values(matrix);
  for (let index = 0; index < expected.length; index++) {
    expect(actual[index]).to.be.closeTo(expected[index], 0.00001);
  }
}

describe('Matrix', () => {
  let PAG: PAG;
  let PAGTypes: typeof Libpag.types;
  beforeEach(() => {
    visitTestPage();
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: typeof Libpag }) => {
      PAG = await window.libpag.PAGInit();
      PAGTypes = window.libpag.types;
    });
  });

  it('Make from all', () => {
    const matrix = PAG.Matrix.makeAll(1, 0, 0, 0, 1, 0);
    expect(values(matrix)).to.be.eql([1, 0, 0, 1, 0, 0]);
  });

  it('Make from scale', () => {
    let matrix = PAG.Matrix.makeScale(1, 1);
    expect(values(matrix)).to.be.eql([1, 0, 0, 1, 0, 0]);
    matrix.destroy();
    matrix = PAG.Matrix.makeScale(0.5, 0.5);
    expect(values(matrix)).to.be.eql([0.5, 0, 0, 0.5, 0, 0]);
  });

  it('Make from trans', () => {
    const matrix = PAG.Matrix.makeTrans(1, 1);
    expect(values(matrix)).to.be.eql([1, 0, 0, 1, 1, 1]);
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
    matrix.setAll(2, 1, 3, 1, 2, 3);
    expect(values(matrix)).to.be.eql([2, 1, 1, 2, 3, 3]);
  });

  it('Set affine', () => {
    const matrix = PAG.Matrix.makeAll(1, 0, 0, 0, 1, 0);
    matrix.setAffine(1, 2, 2, 1, 3, 3);
    expect(values(matrix)).to.be.eql([1, 2, 2, 1, 3, 3]);
  });

  it('Reset', () => {
    const matrix = PAG.Matrix.makeAll(1, 1, 1, 1, 1, 1);
    matrix.reset();
    expect(values(matrix)).to.be.eql([1, 0, 0, 1, 0, 0]);
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
    expectMatrix(matrix, [0, 1, -1, 0, 0, 0]);
  });

  it('Set sin/cos', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    matrix.setSinCos(90, 90);
    expect(values(matrix)).to.be.eql([90, 90, -90, 90, 0, -0]);
  });

  it('Set skew', () => {
    const matrix = PAG.Matrix.makeScale(1, 1);
    matrix.setSkew(30, 30);
    expect(values(matrix)).to.be.eql([1, 30, 30, 1, -0, -0]);
  });

  it('Set concat', () => {
    const matrix = PAG.Matrix.makeAll(1, 2, 3, 1, 2, 3);
    const matrix2 = PAG.Matrix.makeTrans(1, 1);
    matrix.setConcat(matrix, matrix2);
    expect(values(matrix)).to.be.eql([1, 1, 2, 2, 6, 6]);
  });

  // makeAll(scaleX, skewX, transX, skewY, scaleY, transY) maps to getters as
  // a=scaleX, b=skewY, c=skewX, d=scaleY, so this base reads as (2, 0.5, 1, 0.5, 3, 7).
  it('Pre translate', () => {
    const matrix = PAG.Matrix.makeAll(2, 1, 3, 0.5, 0.5, 7);
    matrix.preTranslate(11, 13);
    expectMatrix(matrix, [2, 0.5, 1, 0.5, 38, 19]);
  });

  it('Pre scale', () => {
    const matrix = PAG.Matrix.makeAll(2, 1, 3, 0.5, 0.5, 7);
    matrix.preScale(2, 3);
    expectMatrix(matrix, [4, 1, 3, 1.5, 3, 7]);
  });

  it('Pre rotate', () => {
    const matrix = PAG.Matrix.makeAll(2, 1, 3, 0.5, 0.5, 7);
    matrix.preRotate(90);
    expectMatrix(matrix, [1, 0.5, -2, -0.5, 3, 7]);
  });

  it('Pre skew', () => {
    const matrix = PAG.Matrix.makeAll(2, 1, 3, 0.5, 0.5, 7);
    matrix.preSkew(2, 3);
    expectMatrix(matrix, [5, 2, 5, 1.5, 3, 7]);
  });

  it('Pre concat', () => {
    const matrix = PAG.Matrix.makeAll(2, 1, 3, 0.5, 0.5, 7);
    const other = PAG.Matrix.makeAll(0.5, 2, 0.25, 3, 1, 0.75);
    matrix.preConcat(other);
    expectMatrix(matrix, [4, 1.75, 5, 1.5, 4.25, 7.5]);
  });

  it('Pose translate', () => {
    const matrix = PAG.Matrix.makeAll(2, 1, 3, 0.5, 0.5, 7);
    matrix.postTranslate(11, 13);
    expectMatrix(matrix, [2, 0.5, 1, 0.5, 14, 20]);
  });

  it('Pose scale', () => {
    const matrix = PAG.Matrix.makeAll(2, 1, 3, 0.5, 0.5, 7);
    matrix.postScale(2, 3);
    expectMatrix(matrix, [4, 1.5, 2, 1.5, 6, 21]);
  });

  it('Pose rotate', () => {
    const matrix = PAG.Matrix.makeAll(2, 1, 3, 0.5, 0.5, 7);
    matrix.postRotate(90);
    expectMatrix(matrix, [-0.5, 2, -0.5, 1, -7, 3]);
  });

  it('Pose skew', () => {
    const matrix = PAG.Matrix.makeAll(2, 1, 3, 0.5, 0.5, 7);
    matrix.postSkew(2, 3);
    expectMatrix(matrix, [3, 6.5, 2, 3.5, 17, 16]);
  });

  it('Post concat', () => {
    const matrix = PAG.Matrix.makeAll(2, 1, 3, 0.5, 0.5, 7);
    const other = PAG.Matrix.makeAll(0.5, 2, 0.25, 3, 1, 0.75);
    matrix.postConcat(other);
    expectMatrix(matrix, [2, 6.5, 1.5, 3.5, 15.75, 16.75]);
  });
});
