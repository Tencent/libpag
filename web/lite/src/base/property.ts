import { TimeRange } from './time-range';

export class Property<T> {
  public value: T;

  public constructor(value: T) {
    this.value = value;
  }

  public animatable(): boolean {
    return false;
  }

  public excludeVaryingRanges(_timeRanges: Array<TimeRange>): void {}

  public gotoFrame(_time: number): void {}
}
