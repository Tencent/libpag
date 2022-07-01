import { PAGModule } from './binding';
import { PAGLayer } from './pag-layer';
import { destroyVerify, wasmAwaitRewind } from './utils/decorators';

import type { Color } from './types';

@destroyVerify
@wasmAwaitRewind
export class PAGSolidLayer extends PAGLayer {
  /**
   * [Deprecated]
   * Make a empty PAGSolidLayer with specified size.
   */
  public static Make(duration: number, width: number, height: number, solidColor: Color, opacity: number) {
    console.warn(
      'Please use PAGSolidLayer.make to create PAGSolidLayer object! This interface will be removed in the next version!',
    );
    return PAGSolidLayer.make(duration, width, height, solidColor, opacity);
  }
  /**
   * Make a empty PAGSolidLayer with specified size.
   */
  public static make(duration: number, width: number, height: number, solidColor: Color, opacity: number) {
    const wasmIns = PAGModule._PAGSolidLayer._Make(duration, width, height, solidColor, opacity);
    if (!wasmIns) throw new Error('Make PAGSolidLayer fail!');
    return new PAGSolidLayer(wasmIns);
  }
  /**
   * Returns the layer's solid color.
   */
  public solidColor(): Color {
    return this.wasmIns._solidColor() as Color;
  }
  /**
   * Set the the layer's solid color.
   */
  public setSolidColor(color: Color) {
    this.wasmIns._setSolidColor(color);
  }
}
