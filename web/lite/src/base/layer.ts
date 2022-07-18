import { TimeRange } from './time-range';
import { VectorComposition } from './vector-composition';
import { Ratio, DefaultRatio } from './ratio';
import { BlendMode, ZERO_ID, ZERO_TIME } from '../constant';
import { Transform2D } from './transform-2d';
import { Property } from './property';
import { Mask } from './mask';
import { Effect } from './effects/effect';
import { Point } from './point';
import { verifyFailed } from './utils/verify';
import { AnimatableProperty } from './animatable-property';

export const enum LayerStyleType {
  Unknown,
  DropShadow,
  Stroke,
}

export class LayerStyle {
  public type(): LayerStyleType {
    return LayerStyleType.Unknown;
  }

  public excludeVaryingRanges(_timeRanges: Array<TimeRange>): void {}

  public gotoFrame(_time: number): void {}

  public verify(): boolean {
    return false;
  }
}

export const enum TrackMatteType {
  None = 0,
  Alpha = 1,
  AlphaInverted = 2,
  Luma = 3,
  LumaInverted = 4,
}

export const enum LayerType {
  Unknown,
  undefined,
  Solid,
  Text,
  Shape,
  Image,
  PreCompose,
}

export class Layer {
  /**
   * Te id of the layer.
   */
  public id = 0;
  /**
   * The parent of this layer.
   */
  public parent: Layer | undefined = undefined; // layer reference

  public containingComposition: VectorComposition | undefined = undefined; // composition reference

  /**
   * The time stretch percentage of the layer.
   */
  public stretch: Ratio = DefaultRatio;
  /**
   * The start time of the layer, indicates the start position of the visible range. It could be a negative value.
   */
  public startTime = ZERO_ID;
  /**
   * The duration of the layer, indicates the length of the visible range.
   */
  public duration: number = ZERO_TIME;
  /**
   * When true, the layer' automatic orientation is enabled.
   */
  public autoOrientation = false;
  /**
   * The transformation of the layer.
   */
  public transform: Transform2D | undefined = undefined;
  /**
   * When false, the layer should be skipped during the rendering loop.
   */
  public isActive = true;
  /**
   * The blending mode of the layer.
   */
  public blendMode: BlendMode = BlendMode.Normal;
  /**
   * If layer has a track matte, specifies the way it is applied.
   */
  public trackMatteType: TrackMatteType = TrackMatteType.None;
  public trackMatteLayer: Layer | undefined = undefined;
  public timeRemap: Property<number> | undefined = undefined;
  public masks: Array<Mask> | undefined = undefined;
  public effects: Array<Effect> | undefined = undefined;
  public layerStyles: Array<LayerStyle> | undefined = undefined;

  public layerCache: Cache | undefined = undefined;

  private maxScale: Point | undefined = undefined;

  public type(): LayerType {
    return LayerType.Unknown;
  }

  public excludeVaryingRanges(timeRanges: Array<TimeRange>): void {
    this.transform?.excludeVaryingRanges(timeRanges);
    if (this.timeRemap !== undefined) {
      this.timeRemap.excludeVaryingRanges(timeRanges);
    }
    if (this.masks !== undefined) {
      for (const mask of this.masks) {
        mask.excludeVaryingRanges(timeRanges);
      }
    }
    if (this.effects !== undefined && this.effects.length > 0) {
      for (const effect of this.effects) {
        effect.excludeVaryingRanges(timeRanges);
      }
    }
    if (this.layerStyles !== undefined && this.layerStyles.length > 0) {
      for (const layerStyle of this.layerStyles) {
        layerStyle.excludeVaryingRanges(timeRanges);
      }
    }
  }

  public gotoFrame(frame: number): void {
    this.transform?.gotoFrame(frame);
    if (this.timeRemap !== undefined) {
      this.timeRemap.gotoFrame(frame);
    }
    if (this.masks !== undefined && this.masks.length > 0) {
      for (const mask of this.masks) {
        mask.gotoFrame(frame);
      }
    }
    if (this.effects !== undefined && this.effects.length > 0) {
      for (const effect of this.effects) {
        effect.gotoFrame(frame);
      }
    }
    if (this.layerStyles !== undefined && this.layerStyles.length > 0) {
      for (const layerStyle of this.layerStyles) {
        layerStyle.gotoFrame(frame);
      }
    }
  }

  public verify(): boolean {
    if (!this.containingComposition || this.duration <= 0 || !this.transform) {
      verifyFailed();
      return false;
    }
    if (!this.transform.verify()) {
      verifyFailed();
      return false;
    }
    if (this.masks && this.masks.length > 0) {
      for (const mask of this.masks) {
        if (!mask || !mask.verify()) {
          verifyFailed();
          return false;
        }
      }
    }

    if (this.layerStyles && this.layerStyles.length > 0) {
      for (const layerStyle of this.layerStyles) {
        if (!layerStyle || !layerStyle.verify()) {
          verifyFailed();
          return false;
        }
      }
    }

    if (this.effects && this.effects.length > 0) {
      for (const effect of this.effects) {
        if (!effect || !effect.verify()) {
          verifyFailed();
          return false;
        }
      }
    }

    return true;
  }

  public getMaxScaleFactor(): Point {
    if (this.maxScale !== undefined) {
      return this.maxScale;
    }
    this.maxScale = new Point(1, 1);
    const property = this.transform!.scale;
    if (property.animatable()) {
      const { keyframes } = property as AnimatableProperty<Point>;
      let scaleX = Math.abs(keyframes[0].startValue!.x);
      let scaleY = Math.abs(keyframes[0].startValue!.y);
      if (keyframes !== undefined && keyframes.length > 0) {
        for (const keyframe of keyframes) {
          const x = Math.abs(keyframe.endValue!.x);
          const y = Math.abs(keyframe.endValue!.y);
          if (scaleX < x) {
            scaleX = x;
          }
          if (scaleY < y) {
            scaleY = y;
          }
        }
      }
      this.maxScale.x = scaleX;
      this.maxScale.y = scaleY;
    } else {
      this.maxScale.x = Math.abs(property.value.x);
      this.maxScale.y = Math.abs(property.value.y);
    }
    if (this.parent !== undefined) {
      const parentScale = this.parent.getMaxScaleFactor();
      this.maxScale.x *= parentScale.x;
      this.maxScale.y *= parentScale.y;
    }
    return this.maxScale;
  }
}
