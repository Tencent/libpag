import { Mask } from '../mask';
import { Property } from '../property';
import { TimeRange } from '../time-range';

export class TextPathOptions {
  public path: Mask | undefined; // mask reference
  public reversedPath: Property<boolean> | undefined;
  public perpendicularToPath: Property<boolean> | undefined;
  public forceAlignment: Property<boolean> | undefined;
  public firstMargin: Property<number> | undefined;
  public lastMargin: Property<number> | undefined;

  public excludeVaryingRanges(_timeRanges: Array<TimeRange>): void {}

  public gotoFrame(_time: number): void {}

  public verify(): boolean {
    return false;
  }
}
