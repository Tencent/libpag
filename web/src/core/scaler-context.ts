import { NativeImage } from './native-image';

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
    const metrics = context.measureText(text);
    const bounds = Object();
    bounds.left = Math.floor(-metrics.actualBoundingBoxLeft);
    bounds.top = Math.floor(-metrics.actualBoundingBoxAscent);
    bounds.right = Math.ceil(metrics.actualBoundingBoxRight);
    bounds.bottom = Math.ceil(metrics.actualBoundingBoxDescent);
    return bounds;
  }

  public generateFontMetrics() {
    const { context } = ScalerContext;
    context.font = this.fontString();
    const metrics = context.measureText('中');
    const capHeight = metrics.actualBoundingBoxAscent;
    const xMetrics = context.measureText('x');
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
}
