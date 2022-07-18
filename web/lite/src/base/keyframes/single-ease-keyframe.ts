import { KeyframeInterpolationType } from '../../constant';
import { Keyframe } from '../keyframe';
import { interpolateFloat } from '../utils/interpolate';
import { Interpolator } from '../utils/interpolator';

export class SingleEaseKeyframe<T> extends Keyframe<T> {
  private interpolator: Interpolator | undefined;

  public initialize() {
    if (this.interpolationType === KeyframeInterpolationType.Bezier) {
      // Bazier相关
    } else {
      this.interpolator = new Interpolator();
    }
  }

  public getProgress(time: number): number {
    const progress = (time - this.startTime) / (this.endTime - this.startTime);
    return this.interpolator?.getInterpolation(progress) ?? progress;
  }

  public getValue(time: number): number {
    const progress = this.getProgress(time);
    return interpolateFloat(this.startValue as any as number, this.endValue as any as number, progress);
  }
}
