import { ZERO_TIME } from '../constant';
import { Composition } from './composition';
import { Layer, LayerType } from './layer';
import { TimeRange } from './time-range';
import { Transform2D } from './transform-2d';

export class PreComposeLayer extends Layer {
  public static wrap(composition: Composition) {
    const layer = new PreComposeLayer();
    layer.duration = composition.duration;
    const transform = new Transform2D();
    layer.transform = transform;
    layer.composition = composition;
    return layer;
  }

  /**
   * composition reference
   */
  public composition: Composition | undefined = undefined;

  /**
   * Indicates when the first frame of the composition shows in the layer's timeline. It could be a negative value.
   */
  public compositionStartTime: number = ZERO_TIME;

  private staticTimeRanges: Array<TimeRange> | undefined = undefined;
  private staticTimeRangeUpdated = false;

  public type(): LayerType {
    return LayerType.PreCompose;
  }

  public excludeVaryingRanges(timeRanges: Array<TimeRange>): void {
    super.excludeVaryingRanges(timeRanges);
    if (!timeRanges || timeRanges.length === 0) {
      return;
    }
    this.updateStaticTimeRanges();
  }

  public gotoFrame(frame: number): void {
    super.gotoFrame(frame);
  }

  public verify(): boolean {
    if (!super.verify()) {
      return false;
    }
    if (this.composition) {
      return true;
    }
    return false;
  }

  private updateStaticTimeRanges(): void {
    if (this.staticTimeRangeUpdated) {
      return;
    }
    this.staticTimeRangeUpdated = true;
    const ranges = this.composition?.getStaticTimeRanges();
    if (!ranges) return;
    for (let i = ranges.length - 1; i >= 0; i--) {
      const range: TimeRange = ranges[i];
      range.start += this.compositionStartTime;
      range.end += this.compositionStartTime;
      if (range.end <= this.startTime) {
        ranges.pop();
      } else if (range.start < this.startTime) {
        range.start = 0;
      } else if (range.start >= this.startTime + this.duration - 1) {
        ranges.pop();
      } else if (range.end > this.startTime + this.duration - 1) {
        range.end = this.startTime + this.duration - 1;
      }
    }
    this.staticTimeRanges = ranges;
  }
}
