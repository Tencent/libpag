import { PAG } from './types';
import { PAGLayer } from './pag-layer';
import { wasmAwaitRewind } from './utils/decorators';

@wasmAwaitRewind
export class PAGComposition extends PAGLayer {
  public static module: PAG;

  public constructor(wasmIns) {
    super(wasmIns);
  }
  /**
   * Returns the width of the Composition.
   */
  public width(): number {
    return this.wasmIns._width() as number;
  }
  /**
   * Returns the height of the Composition.
   */
  public height(): number {
    return this.wasmIns._height() as number;
  }
}
