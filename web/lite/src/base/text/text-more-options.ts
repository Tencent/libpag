import { Point } from '../point';
import { Property } from '../property';
import { TimeRange } from '../time-range';

enum AnchorPointGrouping {
  Character = 0,
  Word = 1,
  Line = 2,
  All = 3,
}

export class TextMoreOptions {
  public anchorPointGrouping: AnchorPointGrouping = AnchorPointGrouping.Character;
  public groupingAlignment: Property<Point> | undefined; // multidimensional Percent

  public excludeVaryingRanges(_timeRanges: Array<TimeRange>): void {}

  public gotoFrame(_time: number): void {}

  public verify(): boolean {
    return false;
  }
}
