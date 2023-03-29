import { PAGModule } from '../pag-module';
import { ctor, Point, Vector } from '../types';
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
  public static create(canvas: HTMLCanvasElement | OffscreenCanvas) {
    return new WebMask(canvas);
  }

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

  protected canvas: HTMLCanvasElement | OffscreenCanvas;
  private context: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D;

  public constructor(canvas: HTMLCanvasElement | OffscreenCanvas) {
    this.canvas = canvas;
    this.context = this.canvas.getContext('2d') as CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D;
  }

  public updateCanvas(canvas: HTMLCanvasElement | OffscreenCanvas) {
    this.canvas = canvas;
  }

  public fillPath(path: Path2D, fillType: ctor) {
    this.context.setTransform(1, 0, 0, 1, 0, 0);
    if (
      fillType === PAGModule.TGFXPathFillType.InverseWinding ||
      fillType === PAGModule.TGFXPathFillType.InverseEvenOdd
    ) {
      this.context.clip(path, fillType === PAGModule.TGFXPathFillType.InverseEvenOdd ? 'evenodd' : 'nonzero');
      this.context.fillRect(0, 0, this.canvas.width, this.canvas.height);
    } else {
      this.context.fill(path, fillType === PAGModule.TGFXPathFillType.EvenOdd ? 'evenodd' : 'nonzero');
    }
  }

  public fillText(webFont: WebFont, texts: Vector<string>, positions: Vector<Point>, matrixWasmIns: any) {
    const scalerContext = new ScalerContext(webFont.name, webFont.style, webFont.size, webFont.bold, webFont.italic);
    const matrix = new Matrix(matrixWasmIns);
    this.context.setTransform(matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty);
    this.context.font = scalerContext.fontString();
    for (let i = 0; i < texts.size(); i++) {
      const position = positions.get(i);
      this.context.fillText(texts.get(i), position.x, position.y);
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
    const matrix = new Matrix(matrixWasmIns);
    this.context.setTransform(matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty);
    this.context.font = scalerContext.fontString();
    this.context.lineJoin = WebMask.getLineJoin(stroke.join);
    this.context.miterLimit = stroke.miterLimit;
    this.context.lineCap = WebMask.getLineCap(stroke.cap);
    this.context.lineWidth = stroke.width;
    for (let i = 0; i < texts.size(); i++) {
      const position = positions.get(i);
      this.context.strokeText(texts.get(i), position.x, position.y);
    }
  }

  public clear() {
    this.context.clearRect(0, 0, this.canvas.width, this.canvas.height);
  }
}
