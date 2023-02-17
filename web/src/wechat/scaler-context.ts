import { ScalerContext as NativeScaleContext } from '../core/scaler-context';
import { getCanvas2D } from './canvas';

import type { Rect } from '../types';

export class ScalerContext extends NativeScaleContext {
  public generateImage(text: string, bounds: Rect) {
    const canvas = getCanvas2D();
    canvas.width = bounds.right - bounds.left;
    canvas.height = bounds.bottom - bounds.top;
    const context = canvas.getContext('2d') as OffscreenCanvasRenderingContext2D;
    context.font = this.fontString();
    context.fillText(text, -bounds.left, -bounds.top);
    return canvas;
  }

  protected override loadCanvas() {
    if (!ScalerContext.canvas) {
      ScalerContext.setCanvas(getCanvas2D());
      (ScalerContext.canvas as HTMLCanvasElement | OffscreenCanvas).width = 10;
      (ScalerContext.canvas as HTMLCanvasElement | OffscreenCanvas).height = 10;
      ScalerContext.setContext(
        (ScalerContext.canvas as HTMLCanvasElement | OffscreenCanvas).getContext(
          '2d',
        ) as OffscreenCanvasRenderingContext2D,
      );
    }
  }
}
