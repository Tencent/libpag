import { Color } from '../base/color';
import { Composition } from '../base/composition';
import { Keyframe } from '../base/keyframe';
import { MultiDimensionPointKeyframe } from '../base/keyframes/multi-dimension-point-keyframe';
import { SingleEaseKeyframe } from '../base/keyframes/single-ease-keyframe';
import { Layer } from '../base/layer';
import { Point } from '../base/point';
import { Ratio } from '../base/ratio';
import { AttributeFlag, AttributeType, BaseAttribute, readAttribute } from './attribute-helper';
import {
  readColor,
  readCompositionID,
  readLayerID,
  readPoint,
  readRatio,
  readTime,
  SPATIAL_PRECISION,
} from './data-types';
import { ByteArray } from './utils/byte-array';

export interface BaseAttributeConfig<T> {
  newKeyframe: (flag: AttributeFlag) => Keyframe<T>;
}

export class FloatAttributeConfig extends BaseAttribute implements BaseAttributeConfig<number> {
  public constructor(key: string, attributeType: AttributeType, defaultValue: any) {
    super(key, attributeType, defaultValue);
  }

  public readAttribute(byteArray: ByteArray, flag: AttributeFlag, targetClass: object, target: string) {
    readAttribute(byteArray, flag, targetClass, target, this);
  }

  public readValue(byteArray: ByteArray): number {
    return byteArray.readFloat32();
  }

  public readValueList(byteArray: ByteArray, list: Array<number>, count: number) {
    for (let i = 0; i < count; i++) {
      list.push(this.readValue(byteArray));
    }
  }

  public dimensionality(): number {
    return 1;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<number> {
    return new SingleEaseKeyframe<number>();
  }
}

export class BOOLAttributeConfig extends BaseAttribute implements BaseAttributeConfig<Boolean> {
  public constructor(key: string, attributeType: AttributeType, defaultValue: any) {
    super(key, attributeType, defaultValue);
  }

  public readAttribute(byteArray: ByteArray, flag: AttributeFlag, targetClass: object, target: string) {
    readAttribute(byteArray, flag, targetClass, target, this);
  }

  public readValue(byteArray: ByteArray): boolean {
    return byteArray.readBoolean();
  }

  public readValueList(byteArray: ByteArray, list: Array<boolean>, count: number) {
    for (let i = 0; i < count; i++) {
      list.push(byteArray.readBitBoolean());
    }
  }

  public dimensionality(): number {
    return 1;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<boolean> {
    return new Keyframe<boolean>();
  }
}

export class Uint8AttributeConfig extends BaseAttribute implements BaseAttributeConfig<number> {
  public constructor(key: string, attributeType: AttributeType, defaultValue: number) {
    super(key, attributeType, defaultValue);
  }

  public readAttribute(byteArray: ByteArray, flag: AttributeFlag, targetClass: object, target: string) {
    readAttribute(byteArray, flag, targetClass, target, this);
  }

  public readValue(byteArray: ByteArray): number {
    return byteArray.readUint8();
  }

  public readValueList(byteArray: ByteArray, list: Array<number>, count: number) {
    const valueList = byteArray.readUint32List(count);
    for (let i = 0; i < count; i++) {
      list.push(valueList[i]);
    }
  }

  public dimensionality(): number {
    return 1;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<number> {
    return new SingleEaseKeyframe<number>();
  }
}

export class AttributeConfigUint32 extends BaseAttribute implements BaseAttributeConfig<number> {
  public constructor(key: string, attributeType: AttributeType, defaultValue: any) {
    super(key, attributeType, defaultValue);
  }

  public readAttribute(byteArray: ByteArray, flag: AttributeFlag, targetClass: object, target: string) {
    readAttribute(byteArray, flag, targetClass, target, this);
  }

  public readValue(byteArray: ByteArray): number {
    return byteArray.readEncodedUint32();
  }

  public readValueList(byteArray: ByteArray, list: Array<number>, count: number) {
    // eslint-disable-next-line no-param-reassign
    list = byteArray.readUint32List(count);
  }

  public dimensionality(): number {
    return 1;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<number> {
    return new SingleEaseKeyframe<number>();
  }
}

export class TimeAttributeConfig extends BaseAttribute implements BaseAttributeConfig<number> {
  public constructor(key: string, attributeType: AttributeType, defaultValue: any) {
    super(key, attributeType, defaultValue);
  }

  public readAttribute(byteArray: ByteArray, flag: AttributeFlag, targetClass: object, target: string) {
    readAttribute(byteArray, flag, targetClass, target, this);
  }

  public readValue(byteArray: ByteArray): number {
    return readTime(byteArray);
  }

  public readValueList(byteArray: ByteArray, list: Array<number>, count: number) {
    for (let i = 0; i < count; i++) {
      list[i] = this.readValue(byteArray);
    }
  }

  public dimensionality(): number {
    return 1;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<number> {
    return new SingleEaseKeyframe<number>();
  }
}

export class PointAttributeConfig extends BaseAttribute implements BaseAttributeConfig<Point> {
  public constructor(key: string, attributeType: AttributeType, defaultValue: any) {
    super(key, attributeType, defaultValue);
  }

  public readAttribute(byteArray: ByteArray, flag: AttributeFlag, targetClass: object, target: string) {
    readAttribute(byteArray, flag, targetClass, target, this);
  }

  public readValue(byteArray: ByteArray): Point {
    return readPoint(byteArray);
  }

  public readValueList(byteArray: ByteArray, list: Array<Point>, count: number) {
    if (this.attributeType === AttributeType.SpatialProperty) {
      const values: number[] = byteArray.readFloatList(count * 2, SPATIAL_PRECISION);
      for (let i = 0; i < count; i++) {
        list[i] || (list[i] = new Point(0, 0));
        list[i].x = values[i];
      }
    } else {
      for (let i = 0; i < count; i++) {
        list[i] = readPoint(byteArray);
      }
    }
  }

  public dimensionality(): number {
    return 2;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<Point> {
    switch (this.attributeType) {
      case AttributeType.MultiDimensionProperty:
        return new MultiDimensionPointKeyframe();
      default:
        return new SingleEaseKeyframe<Point>();
    }
  }
}

export class ColorAttributeConfig extends BaseAttribute implements BaseAttributeConfig<Color> {
  public constructor(key: string, attributeType: AttributeType, defaultValue: any) {
    super(key, attributeType, defaultValue);
  }

  public readAttribute(byteArray: ByteArray, flag: AttributeFlag, targetClass: object, target: string) {
    readAttribute(byteArray, flag, targetClass, target, this);
  }

  public readValue(byteArray: ByteArray): Color {
    return readColor(byteArray);
  }

  public readValueList(byteArray: ByteArray, list: Array<Color>, count: number) {
    for (let i = 0; i < count; i++) {
      list[i] = this.readValue(byteArray);
    }
  }

  public dimensionality(): number {
    return 3;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<Color> {
    return new SingleEaseKeyframe<Color>();
  }
}

export class RatioAttributeConfig extends BaseAttribute implements BaseAttributeConfig<Ratio> {
  public constructor(key: string, attributeType: AttributeType, defaultValue: any) {
    super(key, attributeType, defaultValue);
  }

  public readAttribute(byteArray: ByteArray, flag: AttributeFlag, targetClass: object, target: string) {
    readAttribute(byteArray, flag, targetClass, target, this);
  }

  public readValue(byteArray: ByteArray): Ratio {
    return readRatio(byteArray);
  }

  public readValueList(byteArray: ByteArray, list: Array<Ratio>, count: number) {
    for (let i = 0; i < count; i++) {
      list[i] = this.readValue(byteArray);
    }
  }

  public dimensionality(): number {
    return 1;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<Ratio> {
    return new SingleEaseKeyframe<Ratio>();
  }
}

export class StringAttributeConfig extends BaseAttribute implements BaseAttributeConfig<string> {
  public constructor(key: string, attributeType: AttributeType, defaultValue: any) {
    super(key, attributeType, defaultValue);
  }

  public readAttribute(byteArray: ByteArray, flag: AttributeFlag, targetClass: object, target: string) {
    readAttribute(byteArray, flag, targetClass, target, this);
  }

  public readValue(byteArray: ByteArray): string {
    return byteArray.readUTF8String();
  }

  public readValueList(byteArray: ByteArray, list: Array<string>, count: number) {
    for (let i = 0; i < count; i++) {
      list[i] = this.readValue(byteArray);
    }
  }

  public dimensionality(): number {
    return 1;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<string> {
    return new SingleEaseKeyframe<string>();
  }
}

export class LayerAttributeConfig extends BaseAttribute implements BaseAttributeConfig<Layer> {
  public constructor(key: string, attributeType: AttributeType, defaultValue: any) {
    super(key, attributeType, defaultValue);
  }

  public readAttribute(byteArray: ByteArray, flag: AttributeFlag, targetClass: object, target: string) {
    readAttribute(byteArray, flag, targetClass, target, this);
  }

  public readValue(byteArray: ByteArray): Layer {
    return readLayerID(byteArray);
  }

  public readValueList(byteArray: ByteArray, list: Array<Layer>, count: number) {
    for (let i = 0; i < count; i++) {
      list[i] = this.readValue(byteArray);
    }
  }

  public dimensionality(): number {
    return 1;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<Layer> {
    return new SingleEaseKeyframe<Layer>();
  }
}

export class CompositionAttributeConfig extends BaseAttribute implements BaseAttributeConfig<Composition> {
  public constructor(key: string, attributeType: AttributeType, defaultValue: any) {
    super(key, attributeType, defaultValue);
  }

  public readAttribute(byteArray: ByteArray, flag: AttributeFlag, targetClass: object, target: string) {
    readAttribute(byteArray, flag, targetClass, target, this);
  }

  public readValue(byteArray: ByteArray): Composition {
    return readCompositionID(byteArray);
  }

  public readValueList(byteArray: ByteArray, list: Array<Composition>, count: number) {
    for (let i = 0; i < count; i++) {
      list[i] = this.readValue(byteArray);
    }
  }

  public dimensionality(): number {
    return 1;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<Composition> {
    return new SingleEaseKeyframe<Composition>();
  }
}
