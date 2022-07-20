import { Layer, LayerType } from './layer';
import { ShapeElement } from './shape-element';
import { TimeRange } from './time-range';

export class ShapeLayer extends Layer {
  private contents: Array<ShapeElement> = [];

  public type(): LayerType {
    return LayerType.Shape;
  }

  public excludeVaryingRanges(timeRanges: Array<TimeRange>) {
    super.excludeVaryingRanges(timeRanges);
    for (const element of this.contents) {
      element.excludeVaryingRanges(timeRanges);
    }
  }

  public gotoFrame(frame: number) {
    super.gotoFrame(frame);
    for (const element of this.contents) {
      element.gotoFrame(frame);
    }
  }

  public verify(): boolean {
    if (!super.verify()) {
      return false;
    }

    for (const element of this.contents) {
      if (element === undefined || !element.verify()) {
        return false;
      }
    }
    return true;
  }
}
