import { ctor, EmscriptenGL, Matrix, PAG, Point, Vector } from '../types';
import { ScalerContext } from './scaler-context';

export class WebMask {
  public static module: PAG;

  private static getLineCap(cap: ctor): CanvasLineCap {
    switch (cap) {
      case this.module.LineCap.Round:
        return 'round';
      case this.module.LineCap.Square:
        return 'square';
      default:
        return 'butt';
    }
  }

  private static getLineJoin(join: ctor): CanvasLineJoin {
    switch (join) {
      case this.module.LineJoin.Round:
        return 'round';
      case this.module.LineJoin.Bevel:
        return 'bevel';
      default:
        return 'miter';
    }
  }

  private readonly canvas: HTMLCanvasElement;

  public constructor(width: number, height: number) {
    this.canvas = document.createElement('canvas');
    this.canvas.width = width;
    this.canvas.height = height;
  }

  public fillPath(path: Path2D, fillType: ctor) {
    const context = this.canvas.getContext('2d') as CanvasRenderingContext2D;
    context.setTransform(1, 0, 0, 1, 0, 0);
    if (
      fillType === WebMask.module.PathFillType.InverseWinding ||
      fillType === WebMask.module.PathFillType.InverseEvenOdd
    ) {
      context.clip(path, fillType === WebMask.module.PathFillType.InverseEvenOdd ? 'evenodd' : 'nonzero');
      context.fillRect(0, 0, this.canvas.width, this.canvas.height);
    } else {
      context.fill(path, fillType === WebMask.module.PathFillType.EvenOdd ? 'evenodd' : 'nonzero');
    }
  }

  public fillText(
    size: number,
    fauxBold: boolean,
    fauxItalic: boolean,
    fontName: string,
    texts: Vector<string>,
    positions: Vector<Point>,
    matrix: Matrix,
  ) {
    const scalerContext = new ScalerContext(fontName, size, fauxBold, fauxItalic);
    const context = this.canvas.getContext('2d') as CanvasRenderingContext2D;
    context.setTransform(matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty);
    context.font = scalerContext.fontString();
    for (let i = 0; i < texts.size(); i++) {
      const position = positions.get(i);
      context.fillText(texts.get(i), position.x, position.y);
    }
  }

  public strokeText(
    size: number,
    fauxBold: boolean,
    fauxItalic: boolean,
    fontName: string,
    stroke: { width: number; cap: ctor; join: ctor; miterLimit: number },
    texts: Vector<string>,
    positions: Vector<Point>,
    matrix: Matrix,
  ) {
    if (stroke.width < 0.5) {
      return;
    }
    const scalerContext = new ScalerContext(fontName, size, fauxBold, fauxItalic);
    const context = this.canvas.getContext('2d') as CanvasRenderingContext2D;
    context.setTransform(matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty);
    context.font = scalerContext.fontString();
    context.lineJoin = WebMask.getLineJoin(stroke.join);
    context.miterLimit = stroke.miterLimit;
    context.lineCap = WebMask.getLineCap(stroke.cap);
    context.lineWidth = stroke.width;
    for (let i = 0; i < texts.size(); i++) {
      const position = positions.get(i);
      context.strokeText(texts.get(i), position.x, position.y);
    }
  }

  public clear() {
    const context = this.canvas.getContext('2d') as CanvasRenderingContext2D;
    context.clearRect(0, 0, this.canvas.width, this.canvas.height);
  }

  public update(GL: EmscriptenGL) {
    const gl = GL.currentContext?.GLctx as WebGLRenderingContext;
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.ALPHA, gl.ALPHA, gl.UNSIGNED_BYTE, this.canvas);
  }
}
