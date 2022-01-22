import { NativeImage } from './native-image';
import { measureText } from '../utils/measure-text';

export interface Bounds {
  top: number;
  left: number;
  bottom: number;
  right: number;
}

const canvas = ((): HTMLCanvasElement | OffscreenCanvas => {
  try {
    const offscreenCanvas = new OffscreenCanvas(0, 0);
    const context = offscreenCanvas.getContext('2d');
    if (context?.measureText) return offscreenCanvas;
    return document.createElement('canvas');
  } catch (err) {
    return document.createElement('canvas');
  }
})();
canvas.width = 10;
canvas.height = 10;

const testCanvas = document.createElement('canvas');
testCanvas.width = 1;
testCanvas.height = 1;
const testContext = testCanvas.getContext('2d');
testContext.textBaseline = 'top';
testContext.font = '100px -no-font-family-here-';
testContext.scale(0.01, 0.01);
testContext.fillStyle = '#000';
testContext.globalCompositeOperation = 'copy';

export class ScalerContext {
  public static canvas: HTMLCanvasElement | OffscreenCanvas = canvas;
  public static context: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D = canvas.getContext('2d');

  public static isEmoji(text: string): boolean {
    testContext.fillText(text, 0, 0);
    const color = testContext.getImageData(0, 0, 1, 1).data.toString();
    return !color.includes('0,0,0,');
  }

  private readonly fontName: string;
  private readonly size: number;
  private readonly fauxBold: boolean;
  private readonly fauxItalic: boolean;

  private fontBoundingBoxMap: { key: string; value: Bounds }[] = [];

  public constructor(fontName: string, size: number, fauxBold = false, fauxItalic = false) {
    this.fontName = fontName;
    this.size = size;
    this.fauxBold = fauxBold;
    this.fauxItalic = fauxItalic;
  }

  public fontString() {
    const attributes = [];
    if (this.fauxBold) {
      attributes.push('bold');
    }
    if (this.fauxItalic) {
      attributes.push('italic');
    }
    attributes.push(`${this.size}px`);
    attributes.push(`${this.fontName}`);
    return attributes.join(' ');
  }

  public getTextAdvance(text: string) {
    const { context } = ScalerContext;
    context.font = this.fontString();
    return context.measureText(text).width;
  }

  public getTextBounds(text: string) {
    const { context } = ScalerContext;
    context.font = this.fontString();
    const metrics = this.measureText(context, text);

    const bounds: Bounds = {
      left: Math.floor(-metrics.actualBoundingBoxLeft),
      top: Math.floor(-metrics.actualBoundingBoxAscent),
      right: Math.ceil(metrics.actualBoundingBoxRight),
      bottom: Math.ceil(metrics.actualBoundingBoxDescent),
    };
    return bounds;
  }

  public generateFontMetrics() {
    const { context } = ScalerContext;
    context.font = this.fontString();
    const metrics = this.measureText(context, '中');
    const capHeight = metrics.actualBoundingBoxAscent;
    const xMetrics = this.measureText(context, 'x');
    const xHeight = xMetrics.actualBoundingBoxAscent;
    return {
      ascent: -metrics.fontBoundingBoxAscent,
      descent: metrics.fontBoundingBoxDescent,
      xHeight,
      capHeight,
    };
  }

  public generateImage(text: string, bounds): NativeImage {
    const canvas = document.createElement('canvas');
    canvas.width = bounds.right - bounds.left;
    canvas.height = bounds.bottom - bounds.top;
    const context = canvas.getContext('2d');
    context.font = this.fontString();
    context.fillText(text, -bounds.left, -bounds.top);
    return new NativeImage(canvas);
  }

  private measureText(ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D, text: string) {
    const metrics = ctx.measureText(text);
    if (metrics?.actualBoundingBoxAscent) return metrics;
    ctx.canvas.width = this.size * 1.5;
    ctx.canvas.height = this.size * 1.5;
    const pos = [0, this.size];
    ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
    ctx.font = this.fontString();
    ctx.fillText(text, pos[0], pos[1]);
    const imageData = ctx.getImageData(0, 0, ctx.canvas.width, ctx.canvas.height);
    const { left, top, right, bottom } = measureText(imageData);
    ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);

    let fontMeasure: Bounds;
    const fontBoundingBox = this.fontBoundingBoxMap.find((item) => item.key === this.fontName);
    if (fontBoundingBox) {
      fontMeasure = fontBoundingBox.value;
    } else {
      ctx.font = this.fontString();
      ctx.fillText('测', pos[0], pos[1]);
      const fontImageData = ctx.getImageData(0, 0, ctx.canvas.width, ctx.canvas.height);
      fontMeasure = measureText(fontImageData);
      this.fontBoundingBoxMap.push({ key: this.fontName, value: fontMeasure });
      ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
    }

    return {
      actualBoundingBoxAscent: pos[1] - top,
      actualBoundingBoxRight: right - pos[0],
      actualBoundingBoxDescent: bottom - pos[1],
      actualBoundingBoxLeft: pos[0] - left,
      fontBoundingBoxAscent: fontMeasure.bottom - fontMeasure.top,
      fontBoundingBoxDescent: 0,
    };
  }
}
