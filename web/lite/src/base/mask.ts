import { ZERO_ID } from '../constant';
import { Path } from './path';
import { Property } from './property';
import { TimeRange } from './time-range';

export const enum MaskMode {
  None = 0,
  Add = 1,
  Subtract = 2,
  Intersect = 3,
  Lighten = 4,
  Darken = 5,
  Difference = 6,
  Accum = 7,
}

export class Mask {
  public id: number = ZERO_ID;
  public inverted = false;
  public maskMode = MaskMode.None;
  public maskPath: Property<Path> | undefined;
  public maskOpacity: Property<number> | undefined;
  public maskExpansion: Property<number> | undefined;

  public excludeVaryingRanges(_timeRanges: Array<TimeRange>): void {}

  public gotoFrame(_frame: number): void {}

  public verify(): boolean {
    return false;
  }
}
