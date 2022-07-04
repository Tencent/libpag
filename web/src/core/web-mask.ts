import { PAGModule } from '../binding';
import { ctor, EmscriptenGL, Point, Vector } from '../types';
import { ScalerContext } from './scaler-context';
import { Matrix } from './matrix';

export interface WebFont {
  name: string;
  style: string;
  size: number;
  bold: boolean;
  italic: boolean;
}

export class WebMask {
  private static getLineCap(cap: ctor): CanvasLineCap {
    switch (cap) {
      case PAGModule.TGFXLineCap.Round:
        return 'round';
      case PAGModule.TGFXLineCap.Square:
        return 'square';
      default:
        return 'butt';
    }
  }

  private static getLineJoin(join: ctor): CanvasLineJoin {
    switch (join) {
      case PAGModule.TGFXLineJoin.Round:
        return 'round';
      case PAGModule.TGFXLineJoin.Bevel:
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
      fillType === PAGModule.TGFXPathFillType.InverseWinding ||
      fillType === PAGModule.TGFXPathFillType.InverseEvenOdd
    ) {
      context.clip(path, fillType === PAGModule.TGFXPathFillType.InverseEvenOdd ? 'evenodd' : 'nonzero');
      context.fillRect(0, 0, this.canvas.width, this.canvas.height);
    } else {
      context.fill(path, fillType === PAGModule.TGFXPathFillType.EvenOdd ? 'evenodd' : 'nonzero');
    }
  }

  public fillText(webFont: WebFont, texts: Vector<string>, positions: Vector<Point>, matrixWasmIns: any) {
    const scalerContext = new ScalerContext(webFont.name, webFont.style, webFont.size, webFont.bold, webFont.italic);
    const context = this.canvas.getContext('2d') as CanvasRenderingContext2D;
    const matrix = new Matrix(matrixWasmIns);
    context.setTransform(matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty);
    context.font = scalerContext.fontString();
    for (let i = 0; i < texts.size(); i++) {
      const position = positions.get(i);
      context.fillText(texts.get(i), position.x, position.y);
    }
  }

  public strokeText(
    webFont: WebFont,
    stroke: { width: number; cap: ctor; join: ctor; miterLimit: number },
    texts: Vector<string>,
    positions: Vector<Point>,
    matrixWasmIns: any,
  ) {
    if (stroke.width < 0.5) {
      return;
    }
    const scalerContext = new ScalerContext(webFont.name, webFont.style, webFont.size, webFont.bold, webFont.italic);
    const context = this.canvas.getContext('2d') as CanvasRenderingContext2D;
    const matrix = new Matrix(matrixWasmIns);
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
