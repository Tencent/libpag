import { Matrix } from '../types';
import { ScalerContext } from './scaler-context';

export class WebMask {
  public static module;

  private static getLineCap(cap: number): CanvasLineCap {
    switch (cap) {
      case 1:
        return 'round';
      case 2:
        return 'square';
      default:
        return 'butt';
    }
  }

  private static getLineJoin(join: number): CanvasLineJoin {
    switch (join) {
      case 1:
        return 'round';
      case 2:
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

  public fillPath(path: Path2D, fillType) {
    const context = this.canvas.getContext('2d');
    if (
      fillType === WebMask.module.PathFillType.INVERSE_WINDING ||
      fillType === WebMask.module.PathFillType.INVERSE_EVEN_ODD
    ) {
      context.clip(path, fillType === WebMask.module.PathFillType.INVERSE_EVEN_ODD ? 'evenodd' : 'nonzero');
      context.fillRect(0, 0, this.canvas.width, this.canvas.height);
    } else {
      context.fill(path, fillType === WebMask.module.PathFillType.EVEN_ODD ? 'evenodd' : 'nonzero');
    }
  }

  public fillText(
    size: number,
    fauxBold: boolean,
    fauxItalic: boolean,
    fontName: string,
    texts,
    positions,
    matrix: Matrix,
  ) {
    const scalerContext = new ScalerContext(fontName, size, fauxBold, fauxItalic);
    const context = this.canvas.getContext('2d');
    context.transform(matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty);
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
    stroke,
    texts,
    positions,
    matrix: Matrix,
  ) {
    if (stroke.width < 0.5) {
      return;
    }
    const scalerContext = new ScalerContext(fontName, size, fauxBold, fauxItalic);
    const context = this.canvas.getContext('2d');
    context.transform(matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty);
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

  public update(GL) {
    const gl = GL.currentContext.GLctx as WebGLRenderingContext;
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.ALPHA, gl.ALPHA, gl.UNSIGNED_BYTE, this.canvas);
  }
}
