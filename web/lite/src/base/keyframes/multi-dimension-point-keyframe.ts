import { KeyframeInterpolationType } from '../../constant';
import { Keyframe } from '../keyframe';
import { Point } from '../point';
import { interpolateFloat } from '../utils/interpolate';
import { Interpolator } from '../utils/interpolator';

export class MultiDimensionPointKeyframe extends Keyframe<Point> {
  private xInterpolator: Interpolator | undefined;
  private yInterpolator: Interpolator | undefined;

  public initialize(): void {
    super.initialize();
    if (this.interpolationType === KeyframeInterpolationType.Bezier) {
      // todo sun bezier
    } else {
      this.xInterpolator = new Interpolator();
      this.yInterpolator = new Interpolator();
    }
  }

  public getValue(time: number): Point {
    const progress = (time - this.startTime) / (this.endTime - this.startTime);
    const xProgress = this.xInterpolator?.getInterpolation(progress) ?? progress;
    const yProgress = this.yInterpolator?.getInterpolation(progress) ?? progress;
    const x = interpolateFloat(this.startValue!.x, this.endValue!.x, xProgress);
    const y = interpolateFloat(this.startValue!.y, this.endValue!.y, yProgress);
    return { x, y };
  }
}
