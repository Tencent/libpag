import { Color } from '../color';
import { Mask } from '../mask';
import { Property } from '../property';
import { TimeRange } from '../time-range';
import { Effect, EffectType } from './effect';

export class StrokeEffect extends Effect {
  public path: Mask | undefined; // mask reference
  public allMasks = false;
  public strokeSequentially: Property<number> | undefined;
  public color: Property<Color> | undefined;
  public brushSize: Property<number> | undefined;

  public brushHardness: Property<number> | undefined;
  public opacity: Property<number> | undefined;
  public start: Property<number> | undefined;
  public end: Property<number> | undefined;
  public spacing: Property<number> | undefined;
  public paintStyle: Property<number> | undefined; // StrokePaintStyle

  public type(): EffectType {
    return EffectType.Stroke;
  }

  public excludeVaryingRanges(_timeRanges: Array<TimeRange>): void {}

  public gotoFrame(_time: number): void {}

  public verify(): boolean {
    return false;
  }
}
