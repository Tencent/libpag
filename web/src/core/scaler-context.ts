import { measureText } from '../utils/measure-text';
import { defaultFontNames, getFontFamilies } from '../utils/font-family';
import { getCanvas2D } from '../utils/canvas';
import { NativeImage } from './native-image';

import type { Rect } from '../types';
import type { NativeImage as NativeImageType } from '../interfaces';

export class ScalerContext {
  public static canvas: HTMLCanvasElement | OffscreenCanvas;
  public static context: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D;

  public static setCanvas(canvas: HTMLCanvasElement | OffscreenCanvas) {
    ScalerContext.canvas = canvas;
  }

  public static setContext(context: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D) {
    ScalerContext.context = context;
  }

  public static isEmoji(text: string): boolean {
    const emojiRegExp = /\p{Extended_Pictographic}|[#*0-9]\uFE0F?\u20E3|[\uD83C\uDDE6-\uD83C\uDDFF]/u;
    return emojiRegExp.test(text);
  }

  private readonly fontName: string;
  private readonly fontStyle: string;
  private readonly size: number;
  private readonly fauxBold: boolean;
  private readonly fauxItalic: boolean;

  private fontBoundingBoxMap: { key: string; value: Rect }[] = [];

  public constructor(fontName: string, fontStyle: string, size: number, fauxBold = false, fauxItalic = false) {
    this.fontName = fontName;
    this.fontStyle = fontStyle;
    this.size = size;
    this.fauxBold = fauxBold;
    this.fauxItalic = fauxItalic;
    this.loadCanvas();
  }

  public fontString() {
    const attributes = [];
    // css font-style
    if (this.fauxItalic) {
      attributes.push('italic');
    }
    // css font-weight
    if (this.fauxBold) {
      attributes.push('bold');
    }
    // css font-size
    attributes.push(`${this.size}px`);
    // css font-family
    const fallbackFontNames = defaultFontNames.concat();
    fallbackFontNames.unshift(...getFontFamilies(this.fontName, this.fontStyle));
    attributes.push(`${fallbackFontNames.join(',')}`);
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

    const bounds: Rect = {
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

  public generateImage(text: string, bounds: Rect): NativeImageType {
    const canvas = getCanvas2D();
    canvas.width = bounds.right - bounds.left;
    canvas.height = bounds.bottom - bounds.top;
    const context = canvas.getContext('2d') as CanvasRenderingContext2D;
    context.font = this.fontString();
    context.fillText(text, -bounds.left, -bounds.top);
    return new NativeImage(canvas, true);
  }

  protected loadCanvas() {
    if (!ScalerContext.canvas) {
      ScalerContext.setCanvas(getCanvas2D());
      (ScalerContext.canvas as HTMLCanvasElement | OffscreenCanvas).width = 10;
      (ScalerContext.canvas as HTMLCanvasElement | OffscreenCanvas).height = 10;
      ScalerContext.setContext(
        (ScalerContext.canvas as HTMLCanvasElement | OffscreenCanvas).getContext('2d') as
          | CanvasRenderingContext2D
          | OffscreenCanvasRenderingContext2D,
      );
    }
  }

  private measureText(ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D, text: string): TextMetrics {
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

    let fontMeasure: Rect;
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
      width: fontMeasure.right - fontMeasure.left,
    };
  }
}
