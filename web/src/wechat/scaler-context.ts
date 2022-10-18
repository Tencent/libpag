import { ScalerContext as NativeScaleContext } from '../core/scaler-context';
import { getCanvas2D } from './canvas';
import { NativeImage } from './native-image';

import type { Rect } from '../types';
import type { NativeImage as NativeImageType } from '../interfaces';

export class ScalerContext extends NativeScaleContext {
  public generateImage(text: string, bounds: Rect): NativeImageType {
    const canvas = getCanvas2D();
    canvas.width = bounds.right - bounds.left;
    canvas.height = bounds.bottom - bounds.top;
    const context = canvas.getContext('2d') as OffscreenCanvasRenderingContext2D;
    context.font = this.fontString();
    context.fillText(text, -bounds.left, -bounds.top);
    return new NativeImage(canvas, true);
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
