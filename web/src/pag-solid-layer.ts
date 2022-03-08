import { PAGLayer } from './pag-layer';
import { Color } from './types';
import { wasmAwaitRewind } from './utils/decorators';

@wasmAwaitRewind
export class PAGSolidLayer extends PAGLayer {
  public static Make(duration: number, width: number, height: number, solidColor: Color, opacity: number) {
    return new PAGSolidLayer(this.module._PAGSolidLayer._Make(duration, width, height, solidColor, opacity));
  }
  public constructor(wasmIns: any) {
    super(wasmIns);
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
