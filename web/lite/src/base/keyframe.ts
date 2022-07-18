import { KeyframeInterpolationType } from '../constant';
import { Point, ZERO_POINT } from './point';

export class Keyframe<T> {
  public startValue: T | undefined;
  public endValue: T | undefined;
  public startTime = 0;
  public endTime = 0;
  public interpolationType: KeyframeInterpolationType = KeyframeInterpolationType.Hold; // 插补类型
  public bezierOut: Array<Point> = [];
  public bezierIn: Array<Point> = [];
  public spatialOut: Point = ZERO_POINT;
  public spatialIn: Point = ZERO_POINT;

  public initialize(): void {}

  public getValue(_time: number): any {
    return this.startValue;
  }

  public containsTime(time: number): boolean {
    return time >= this.startTime && time < this.endTime;
  }
}
