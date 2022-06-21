import { PAGModule } from '../binding';
import { destroyVerify, wasmAwaitRewind } from '../utils/decorators';
import { MatrixIndex } from '../types';

@destroyVerify
@wasmAwaitRewind
export class Matrix {
  /**
   * Sets Matrix to:
   *
   *      | scaleX  skewX transX |
   *      |  skewY scaleY transY |
   *      |  pers0  pers1  pers2 |
   *
   * @param scaleX  horizontal scale factor
   * @param skewX   horizontal skew factor
   * @param transX  horizontal translation
   * @param skewY   vertical skew factor
   * @param scaleY  vertical scale factor
   * @param transY  vertical translation
   * @param pers0   input x-axis perspective factor
   * @param pers1   input y-axis perspective factor
   * @param pers2   perspective scale factor
   * @return        Matrix constructed from parameters
   */
  public static makeAll(
    scaleX: number,
    skewX: number,
    transX: number,
    skewY: number,
    scaleY: number,
    transY: number,
    pers0 = 0,
    pers1 = 0,
    pers2 = 1,
  ): Matrix {
    const wasmIns = PAGModule._Matrix._MakeAll(scaleX, skewX, transX, skewY, scaleY, transY, pers0, pers1, pers2);
    if (!wasmIns) throw new Error('Matrix.makeAll fail, please check parameters valid!');
    return new Matrix(wasmIns);
  }
  /**
   * Sets Matrix to scale by (sx, sy). Returned matrix is:
   *
   *       | sx  0  0 |
   *       |  0 sy  0 |
   *       |  0  0  1 |
   *
   *  @param scaleX  horizontal scale factor
   *  @param scaleY  [optionals] vertical scale factor, default equal scaleX.
   *  @return    Matrix with scale
   */
  public static makeScale(scaleX: number, scaleY?: number): Matrix {
    let wasmIns;
    if (scaleY !== undefined) {
      wasmIns = PAGModule._Matrix._MakeScale(scaleX, scaleY);
    } else {
      wasmIns = PAGModule._Matrix._MakeScale(scaleX);
    }
    if (!wasmIns) throw new Error('Matrix.makeScale fail, please check parameters valid!');
    return new Matrix(wasmIns);
  }
  /**
   * Sets Matrix to translate by (dx, dy). Returned matrix is:
   *
   *       | 1 0 dx |
   *       | 0 1 dy |
   *       | 0 0  1 |
   *
   * @param dx  horizontal translation
   * @param dy  vertical translation
   * @return    Matrix with translation
   */
  public static makeTrans(dx: number, dy: number): Matrix {
    const wasmIns = PAGModule._Matrix._MakeTrans(dx, dy);
    if (!wasmIns) throw new Error('Matrix.makeTrans fail, please check parameters valid!');
    return new Matrix(wasmIns);
  }

  public wasmIns;
  public isDestroyed = false;

  public constructor(wasmIns: any) {
    this.wasmIns = wasmIns;
  }

  /**
   * scaleX; horizontal scale factor to store
   */
  public get a(): number {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.a) : 0;
  }
  public set a(value: number) {
    this.wasmIns?._set(MatrixIndex.a, value);
  }
  /**
   * skewY; vertical skew factor to store
   */
  public get b(): number {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.b) : 0;
  }
  public set b(value: number) {
    this.wasmIns?._set(MatrixIndex.b, value);
  }
  /**
   * skewX; horizontal skew factor to store
   */
  public get c(): number {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.c) : 0;
  }
  public set c(value: number) {
    this.wasmIns?._set(MatrixIndex.c, value);
  }
  /**
   * scaleY; vertical scale factor to store
   */
  public get d(): number {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.d) : 0;
  }
  public set d(value: number) {
    this.wasmIns?._set(MatrixIndex.d, value);
  }
  /**
   * transX; horizontal translation to store
   */
  public get tx(): number {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.tx) : 0;
  }
  public set tx(value: number) {
    this.wasmIns?._set(MatrixIndex.tx, value);
  }
  /**
   * transY; vertical translation to store
   */
  public get ty(): number {
    return this.wasmIns ? this.wasmIns._get(MatrixIndex.ty) : 0;
  }
  public set ty(value: number) {
    this.wasmIns?._set(MatrixIndex.ty, value);
  }

  /**
   * Returns one matrix value.
   */
  public get(index: MatrixIndex): number {
    return this.wasmIns ? this.wasmIns._get(index) : 0;
  }
  /**
   * Sets Matrix value.
   */
  public set(index: MatrixIndex, value: number) {
    this.wasmIns?._set(index, value);
  }
  /**
   * Sets all values from parameters. Sets matrix to:
   *
   *      | scaleX  skewX transX |
   *      |  skewY scaleY transY |
   *      | persp0 persp1 persp2 |
   *
   * @param scaleX  horizontal scale factor to store
   * @param skewX   horizontal skew factor to store
   * @param transX  horizontal translation to store
   * @param skewY   vertical skew factor to store
   * @param scaleY  vertical scale factor to store
   * @param transY  vertical translation to store
   * @param persp0  input x-axis values perspective factor to store
   * @param persp1  input y-axis values perspective factor to store
   * @param persp2  perspective scale factor to store
   */
  public setAll(
    scaleX: number,
    skewX: number,
    transX: number,
    skewY: number,
    scaleY: number,
    transY: number,
    pers0 = 0,
    pers1 = 0,
    pers2 = 1,
  ) {
    this.wasmIns?._setAll(scaleX, skewX, transX, skewY, scaleY, transY, pers0, pers1, pers2);
  }

  public setAffine(a: number, b: number, c: number, d: number, tx: number, ty: number) {
    this.wasmIns?._setAffine(a, b, c, d, tx, ty);
  }
  /**
   * Sets Matrix to identity; which has no effect on mapped Point. Sets Matrix to:
   *
   *       | 1 0 0 |
   *       | 0 1 0 |
   *       | 0 0 1 |
   *
   * Also called setIdentity(); use the one that provides better inline documentation.
   */
  public reset() {
    this.wasmIns?._reset();
  }
  /**
   * Sets Matrix to translate by (dx, dy).
   * @param dx  horizontal translation
   * @param dy  vertical translation
   */
  public setTranslate(dx: number, dy: number) {
    this.wasmIns?._setTranslate(dx, dy);
  }
  /**
   * Sets Matrix to scale by sx and sy, about a pivot point at (px, py). The pivot point is
   * unchanged when mapped with Matrix.
   * @param sx  horizontal scale factor
   * @param sy  vertical scale factor
   * @param px  pivot on x-axis
   * @param py  pivot on y-axis
   */
  public setScale(sx: number, sy: number, px = 0, py = 0) {
    this.wasmIns?._setScale(sx, sy, px, py);
  }
  /**
   * Sets Matrix to rotate by degrees about a pivot point at (px, py). The pivot point is
   * unchanged when mapped with Matrix. Positive degrees rotates clockwise.
   *  @param degrees  angle of axes relative to upright axes
   *  @param px       pivot on x-axis
   *  @param py       pivot on y-axis
   */
  public setRotate(degrees: number, px = 0, py = 0) {
    this.wasmIns?._setRotate(degrees, px, py);
  }
  /**
   * Sets Matrix to rotate by sinValue and cosValue, about a pivot point at (px, py).
   * The pivot point is unchanged when mapped with Matrix.
   * Vector (sinValue, cosValue) describes the angle of rotation relative to (0, 1).
   * Vector length specifies scale.
   */
  public setSinCos(sinV: number, cosV: number, px = 0, py = 0) {
    this.wasmIns?._setSinCos(sinV, cosV, px, py);
  }
  /**
   * Sets Matrix to skew by kx and ky, about a pivot point at (px, py). The pivot point is
   * unchanged when mapped with Matrix.
   * @param kx  horizontal skew factor
   * @param ky  vertical skew factor
   * @param px  pivot on x-axis
   * @param py  pivot on y-axis
   */
  public setSkew(kx: number, ky: number, px = 0, py = 0) {
    this.wasmIns?._setSkew(kx, ky, px, py);
  }
  /**
   * Sets Matrix to Matrix a multiplied by Matrix b. Either a or b may be this.
   *
   * Given:
   *
   *          | A B C |      | J K L |
   *      a = | D E F |, b = | M N O |
   *          | G H I |      | P Q R |
   *
   * sets Matrix to:
   *
   *              | A B C |   | J K L |   | AJ+BM+CP AK+BN+CQ AL+BO+CR |
   *      a * b = | D E F | * | M N O | = | DJ+EM+FP DK+EN+FQ DL+EO+FR |
   *              | G H I |   | P Q R |   | GJ+HM+IP GK+HN+IQ GL+HO+IR |
   *
   * @param a  Matrix on left side of multiply expression
   * @param b  Matrix on right side of multiply expression
   */
  public setConcat(a: Matrix, b: Matrix) {
    this.wasmIns?._setConcat(a.wasmIns, b.wasmIns);
  }
  /**
   * Preconcats the matrix with the specified scale. M' = M * S(sx, sy)
   */
  public preTranslate(dx: number, dy: number) {
    this.wasmIns?._preTranslate(dx, dy);
  }
  /**
   * Postconcats the matrix with the specified scale. M' = S(sx, sy, px, py) * M
   */
  public preScale(sx: number, sy: number, px = 0, py = 0) {
    this.wasmIns?._preScale(sx, sy, px, py);
  }
  /**
   * Preconcats the matrix with the specified rotation. M' = M * R(degrees, px, py)
   */
  public preRotate(degrees: number, px = 0, py = 0) {
    this.wasmIns?._preRotate(degrees, px, py);
  }
  /**
   * Preconcats the matrix with the specified skew. M' = M * K(kx, ky, px, py)
   */
  public preSkew(kx: number, ky: number, px = 0, py = 0) {
    this.wasmIns?._preSkew(kx, ky, px, py);
  }
  /**
   * Preconcats the matrix with the specified matrix. M' = M * other
   */
  public preConcat(other: Matrix) {
    this.wasmIns?._preConcat(other.wasmIns);
  }
  /**
   * Postconcats the matrix with the specified translation. M' = T(dx, dy) * M
   */
  public postTranslate(dx: number, dy: number) {
    this.wasmIns?._postTranslate(dx, dy);
  }
  /**
   * Postconcats the matrix with the specified scale. M' = S(sx, sy, px, py) * M
   */
  public postScale(sx: number, sy: number, px = 0, py = 0) {
    this.wasmIns?._postScale(sx, sy, px, py);
  }
  /**
   * Postconcats the matrix with the specified rotation. M' = R(degrees, px, py) * M
   */
  public postRotate(degrees: number, px = 0, py = 0) {
    this.wasmIns?._postRotate(degrees, px, py);
  }
  /**
   * Postconcats the matrix with the specified skew. M' = K(kx, ky, px, py) * M
   */
  public postSkew(kx: number, ky: number, px = 0, py = 0) {
    this.wasmIns?._postSkew(kx, ky, px, py);
  }
  /**
   * Postconcats the matrix with the specified matrix. M' = other * M
   */
  public postConcat(other: Matrix) {
    this.wasmIns?._postConcat(other.wasmIns);
  }

  public destroy() {
    this.wasmIns.delete();
  }
}
