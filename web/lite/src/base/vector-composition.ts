import { CompositionType } from '../codec/types';
import { Composition } from './composition';
import { Layer } from './layer';
import { splitTimeRangesAt, TimeRange } from './time-range';
import { verifyFailed } from './utils/verify';

export class VectorComposition extends Composition {
  public layers: Array<Layer> = [];

  private staticTimeRanges: Array<TimeRange> = [];
  private staticTimeRangeUpdated = false;

  public type(): CompositionType {
    return CompositionType.Vector;
  }

  /**
   * Returns the static time ranges of this composition.
   */
  public getStaticTimeRanges(): Array<TimeRange> {
    if (!this.staticTimeRangeUpdated) {
      this.staticTimeRangeUpdated = true;
      this.updateStaticTimeRanges();
    }
    return this.staticTimeRanges;
  }

  public verify(): boolean {
    if (!super.verify()) {
      verifyFailed();
      return false;
    }
    for (const layer of this.layers) {
      if (!layer || !layer.verify()) {
        verifyFailed();
        return false;
      }
    }
    return true;
  }

  private updateStaticTimeRanges() {
    if (this.duration > 1) {
      const range = { start: 0, end: this.duration - 1 };
      this.staticTimeRanges = [range];
      for (const layer of this.layers) {
        if (this.staticTimeRanges.length <= 0) {
          break;
        }
        layer.excludeVaryingRanges(this.staticTimeRanges);
        splitTimeRangesAt(this.staticTimeRanges, layer.startTime);
        splitTimeRangesAt(this.staticTimeRanges, layer.startTime + layer.duration);
      }
    }
  }
}
