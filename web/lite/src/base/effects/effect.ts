import { Mask } from '../mask';
import { Property } from '../property';
import { TimeRange } from '../time-range';

export const enum EffectType {
  Unknown,
  Tint,
  Fill,
  Stroke,
  Tritone,
  DropShadow,
  RadialWipe,
  DisplacementMap,
}

export class Effect {
  public effectOpacity: Property<number> | undefined;
  public maskReferences: Array<Mask> | undefined; // mask reference

  public type(): EffectType {
    return EffectType.Unknown;
  }

  public excludeVaryingRanges(_timeRanges: Array<TimeRange>): void {}

  public gotoFrame(_time: number): void {}

  public verify(): boolean {
    return false;
  }
}
