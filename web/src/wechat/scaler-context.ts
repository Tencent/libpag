import { ScalerContext as NativeScaleContext } from '@tgfx/core/scaler-context';
import { getCanvas2D,releaseCanvas2D } from '@tgfx/wechat/canvas';

import type { ctor,Rect } from '../types';

export class ScalerContext extends NativeScaleContext {
  public generateImage(text: string, bounds: Rect) {
    const canvas = getCanvas2D(bounds.right - bounds.left, bounds.bottom - bounds.top);
    const context = canvas.getContext('2d') as OffscreenCanvasRenderingContext2D;
    context.font = this.fontString(false, false);
    context.fillText(text, -bounds.left, -bounds.top);
    return canvas;
  }

  protected override loadCanvas() {
    if (!ScalerContext.canvas) {
      ScalerContext.setCanvas(getCanvas2D(10, 10));
      ScalerContext.setContext(
        (ScalerContext.canvas as HTMLCanvasElement | OffscreenCanvas).getContext(
          '2d',
        ) as OffscreenCanvasRenderingContext2D,
      );
    }
  }

  public override readPixels(
      text: string,
      bounds: Rect,
      fauxBold: boolean,
      stroke ?: { width: number; cap: ctor; join: ctor; miterLimit: number }
  ) {
    const width = bounds.right - bounds.left;
    const height = bounds.bottom - bounds.top
    const canvas = getCanvas2D(width, height);
    const context = canvas.getContext('2d') as OffscreenCanvasRenderingContext2D;
    context.clearRect(0, 0, width, height);
    context.font = this.fontString(fauxBold, false);
    if (stroke){
      context.lineJoin = ScalerContext.getLineJoin(stroke.join);
      context.miterLimit = stroke.miterLimit;
      context.lineCap = ScalerContext.getLineCap(stroke.cap);
      context.lineWidth = stroke.width;
      context.strokeText(text, -bounds.left, -bounds.top);
    } else {
      context.fillText(text, -bounds.left, -bounds.top);
    }
    const {data} = context.getImageData(0, 0, width, height);
    releaseCanvas2D(canvas);
    if (data.length === 0) {
      return null;
    }
    return new Uint8Array(data);
  }

}
