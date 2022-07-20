import { OPAQUE } from '../constant';
import { Point, ZERO_POINT } from './point';
import { Property } from './property';

import type { TimeRange } from './time-range';

export class Transform2D {
  public static createDefaultTransform2D() {
    const transform = new Transform2D();

    return transform;
  }

  public anchorPoint: Property<Point>; // spatial
  public position: Property<Point>; // spatial
  public xPosition: Property<number>;
  public yPosition: Property<number>;
  public scale: Property<Point>; // multidimensional
  public rotation: Property<number>;
  public opacity: Property<number>;

  public constructor() {
    this.anchorPoint = new Property<Point>(ZERO_POINT);
    this.position = new Property<Point>(ZERO_POINT);
    this.xPosition = new Property<number>(0);
    this.yPosition = new Property<number>(0);
    this.scale = new Property<Point>(new Point(1, 1));
    this.rotation = new Property<number>(0);
    this.opacity = new Property<number>(OPAQUE);
  }

  public excludeVaryingRanges(timeRanges: Array<TimeRange>): void {
    this.anchorPoint.excludeVaryingRanges(timeRanges);
    if (this.position !== undefined) {
      this.position.excludeVaryingRanges(timeRanges);
    } else {
      this.xPosition.excludeVaryingRanges(timeRanges);
      this.yPosition.excludeVaryingRanges(timeRanges);
    }
    this.scale.excludeVaryingRanges(timeRanges);
    this.rotation.excludeVaryingRanges(timeRanges);
    this.opacity.excludeVaryingRanges(timeRanges);
  }

  public gotoFrame(frame: number): void {
    this.anchorPoint.gotoFrame(frame);
    if (this.position !== undefined) {
      this.position.gotoFrame(frame);
    } else {
      this.xPosition.gotoFrame(frame);
      this.yPosition.gotoFrame(frame);
    }
    this.scale.gotoFrame(frame);
    this.rotation.gotoFrame(frame);
    this.opacity.gotoFrame(frame);
  }

  public verify(): boolean {
    return (
      this.anchorPoint !== undefined &&
      (this.position !== undefined || (this.xPosition !== undefined && this.yPosition !== undefined)) &&
      this.scale !== undefined &&
      this.rotation !== undefined &&
      this.opacity !== undefined
    );
  }
}
