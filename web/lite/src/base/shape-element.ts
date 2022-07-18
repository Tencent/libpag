import { ShapeType } from './shape-type';
import { TimeRange } from './time-range';

export class ShapeElement {
  public type(): ShapeType {
    return ShapeType.Unknown;
  }

  public excludeVaryingRanges(_timeRanges: Array<TimeRange>) {}

  public gotoFrame(_frame: number) {}

  public verify(): boolean {
    return true;
  }
}
