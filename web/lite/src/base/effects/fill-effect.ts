import { Color } from '../color';
import { Mask } from '../mask';
import { Property } from '../property';
import { TimeRange } from '../time-range';
import { Effect, EffectType } from './effect';

export class FillEffect extends Effect {
  public fillMask: Mask | undefined; // mask reference
  public allMasks = false;
  public color: Property<Color> | undefined;
  public invert: Property<boolean> | undefined;
  public horizontalFeather: Property<number> | undefined;
  public verticalFeather: Property<number> | undefined;
  public opacity: Property<number> | undefined;

  public type(): EffectType {
    return EffectType.Fill;
  }

  public excludeVaryingRanges(_timeRanges: Array<TimeRange>): void {}

  public gotoFrame(_time: number): void {}

  public verify(): boolean {
    return false;
  }
}
