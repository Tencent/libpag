import { KeyframeInterpolationType } from '../constant';
import { Keyframe } from './keyframe';
import { Property } from './property';
import { splitTimeRangesAt, subtractFromTimeRanges, TimeRange } from './time-range';

export class AnimatableProperty<T> extends Property<T> {
  private lastKeyframeIndex = 0;

  public constructor(public keyframes: Array<Keyframe<T>>) {
    if (!keyframes || keyframes.length === 0) throw new Error('keyframes is required');
    if (keyframes[0].startValue === undefined) throw new Error('startValue is required');
    super(keyframes[0].startValue);
    for (const keyframe of keyframes) {
      keyframe.initialize();
    }
  }

  public animatable(): boolean {
    return true;
  }

  public excludeVaryingRanges(timeRanges: Array<TimeRange>): void {
    for (const keyframe of this.keyframes) {
      switch (keyframe.interpolationType) {
        case KeyframeInterpolationType.Bezier:
        case KeyframeInterpolationType.Linear:
          subtractFromTimeRanges(timeRanges, keyframe.startTime, keyframe.endTime - 1);
          break;
        default:
          splitTimeRangesAt(timeRanges, keyframe.startTime);
          splitTimeRangesAt(timeRanges, keyframe.endTime);
          break;
      }
    }
  }

  public gotoFrame(frame: number): void {
    let lastKeyframe = this.keyframes[this.lastKeyframeIndex];
    if (lastKeyframe.containsTime(frame)) {
      this.value = lastKeyframe.getValue(frame);
      return;
    }
    if (frame < lastKeyframe.startTime) {
      while (this.lastKeyframeIndex > 0) {
        this.lastKeyframeIndex -= 1;
        if (this.keyframes[this.lastKeyframeIndex].containsTime(frame)) {
          break;
        }
      }
    } else {
      while (this.lastKeyframeIndex < this.keyframes.length - 1) {
        this.lastKeyframeIndex += 1;
        if (this.keyframes[this.lastKeyframeIndex].containsTime(frame)) {
          break;
        }
      }
    }
    lastKeyframe = this.keyframes[this.lastKeyframeIndex];
    if (lastKeyframe.startValue !== undefined && frame <= lastKeyframe.startTime) {
      this.value = lastKeyframe.startValue;
    } else if (lastKeyframe.endValue !== undefined && frame >= lastKeyframe.endTime) {
      this.value = lastKeyframe.endValue;
    } else {
      this.value = lastKeyframe.getValue(frame);
    }
  }
}
