import { TagCode } from './types';
import { ByteArray } from './utils/byte-array';
import { Keyframe } from '../base/keyframe';
import { Property } from '../base/property';
import { BEZIER_PRECISION, readTime } from './data-types';
import { KeyframeInterpolationType } from '../constant';
import { AnimatableProperty } from '../base/animatable-property';

export const enum AttributeType {
  Value,
  FixedValue, // always exists, no need to store a flag.
  SimpleProperty,
  DiscreteProperty,
  MultiDimensionProperty,
  SpatialProperty,
  BitFlag, // save bool value as a flag
  Custom, // save a flag to indicate whether it should trigger a custom reading / writing action.
}

export interface AttributeFlag {
  /**
   * Indicates whether or not this value is exist.
   */
  exist: boolean;
  /**
   * Indicates whether or not the size of this property's keyframes is greater than zero.
   */
  animatable: boolean;
  /**
   * Indicates whether or not this property has spatial values.
   */
  hasSpatial: boolean;
}

export const readTagBlock = <T>(byteArray: ByteArray, parameter: T, blockConfig: BlockConfig) => {
  const tagConfig: BlockConfig = blockConfig;
  const flags: Array<AttributeFlag> = [];
  if (!tagConfig.configs || tagConfig.configs.length === 0) {
    return parameter;
  }
  for (const config of tagConfig.configs) {
    const flag = readAttributeFlag(byteArray, config);
    flags.push(flag);
  }
  byteArray.alignWithBytes();
  let index = 0;
  for (const config of tagConfig.configs) {
    const flag = flags[index];
    const target = config.key;
    config.readAttribute(byteArray, flag, parameter as any as Object, target);
    index += 1;
  }
  return parameter;
};

export class BlockConfig {
  public tagCode: TagCode = TagCode.End;
  public configs: Array<BaseAttribute> = [];

  public constructor(tagCode: TagCode) {
    this.tagCode = tagCode;
  }
}

export class BaseAttribute {
  public attributeType: AttributeType;
  public defaultValue: any;
  public key: string;

  public constructor(key: string, attributeType: AttributeType, defaultValue: any) {
    this.attributeType = attributeType;
    this.defaultValue = defaultValue;
    this.key = key;
  }

  public readAttribute(_byteArray: ByteArray, _flag: AttributeFlag, _targetClass: object, _target: string) {}

  public readValue(_byteArray: ByteArray): any {
    return undefined;
  }

  public readValueList(_byteArray: ByteArray, _list: Array<any>, _count: number) {}

  public dimensionality(): number {
    return 1;
  }

  public newKeyframe(_flag: AttributeFlag): Keyframe<any> {
    return new Keyframe<any>();
  }
}

export const readAttribute = (
  byteArray: ByteArray,
  flag: AttributeFlag,
  targetClass: { [key: string]: any },
  target: string,
  config: BaseAttribute,
) => {
  if (config.attributeType === AttributeType.BitFlag) {
    targetClass[target] = flag.exist;
  } else if (config.attributeType === AttributeType.FixedValue) {
    targetClass[target] = config.readValue(byteArray);
  } else if (config.attributeType === AttributeType.Value) {
    targetClass[target] = readValue(byteArray, config, flag);
  } else {
    targetClass[target] = readProperty(byteArray, config, flag);
  }
};

export const readProperty = <T>(byteArray: ByteArray, config: BaseAttribute, flag: AttributeFlag): any => {
  let property: Property<T>;
  if (flag.exist) {
    if (flag.animatable) {
      const keyframes: Array<Keyframe<T>> = readKeyframes(byteArray, config, flag);
      if (!keyframes || keyframes.length === 0) {
        throw new Error('Wrong number of keyframes!');
      }
      readTimeAndValue(byteArray, keyframes, config);
      readTimeEase(byteArray, keyframes, config);
      if (flag.hasSpatial) {
        readSpatialEase(byteArray, keyframes);
      }
      property = new AnimatableProperty<T>(keyframes);
    } else {
      property = new Property<T>(readValue(byteArray, config, flag));
    }
  } else {
    property = new Property<T>(config.defaultValue);
  }
  return property;
};

export const readValue = (byteArray: ByteArray, config: BaseAttribute, flag: AttributeFlag): any => {
  if (flag.exist) {
    return config.readValue(byteArray);
  }
  return config.defaultValue;
};

export const readAttributeFlag = (byteArray: ByteArray, config: BaseAttribute): AttributeFlag => {
  const flag: AttributeFlag = { exist: false, animatable: false, hasSpatial: false };
  const { attributeType } = config;
  if (attributeType === AttributeType.FixedValue) {
    flag.exist = true;
    return flag;
  }
  flag.exist = byteArray.readBitBoolean();
  if (
    !flag.exist ||
    attributeType === AttributeType.Value ||
    attributeType === AttributeType.BitFlag ||
    attributeType === AttributeType.Custom
  ) {
    return flag;
  }
  flag.animatable = byteArray.readBitBoolean();
  if (!flag.animatable || attributeType !== AttributeType.SpatialProperty) {
    return flag;
  }
  flag.hasSpatial = byteArray.readBitBoolean();
  return flag;
};

export const readKeyframes = <T>(
  byteArray: ByteArray,
  config: BaseAttribute,
  flag: AttributeFlag,
): Array<Keyframe<T>> => {
  const keyframes: Array<any> = [];
  const numFrames: number = byteArray.readEncodedUint32();
  for (let i = 0; i < numFrames; i++) {
    let keyframe: Keyframe<T>;
    if (config.attributeType === AttributeType.DiscreteProperty) {
      keyframe = new Keyframe<T>();
    } else {
      const interpolationType = byteArray.readUBits(2) as KeyframeInterpolationType;
      if (interpolationType === KeyframeInterpolationType.Hold) {
        keyframe = new Keyframe<T>();
      } else {
        keyframe = config.newKeyframe(flag);
        keyframe.interpolationType = interpolationType;
      }
    }
    keyframes.push(keyframe);
  }

  return keyframes;
};

const readTimeAndValue = <T>(byteArray: ByteArray, keyframes: Array<Keyframe<T>>, config: BaseAttribute) => {
  const numFrames: number = keyframes.length;
  keyframes[0].startTime = readTime(byteArray);
  for (let i = 0; i < numFrames; i++) {
    const time: number = readTime(byteArray);
    keyframes[i].endTime = time;
    if (i < numFrames - 1) {
      keyframes[i + 1].startTime = time;
    }
  }
  const list: Array<T> = [];
  config.readValueList(byteArray, list, numFrames + 1);
  let index = 0;
  keyframes[0].startValue = list[index];
  index += 1;
  for (let i = 0; i < numFrames; i++) {
    const value = list[index];
    index += 1;
    keyframes[i].endValue = value;
    if (i < numFrames - 1) {
      keyframes[i + 1].startValue = value;
    }
  }
};

const readTimeEase = <T>(byteArray: ByteArray, keyframes: Array<Keyframe<T>>, config: BaseAttribute) => {
  const dimensionality: number =
    config.attributeType === AttributeType.MultiDimensionProperty ? config.dimensionality() : 1;
  const numBits: number = byteArray.readNumBits();
  for (const keyframe of keyframes) {
    if (keyframe.interpolationType !== KeyframeInterpolationType.Bezier) {
      continue;
    }
    let x: number;
    let y: number;
    for (let i = 0; i < dimensionality; i++) {
      x = byteArray.readBits(numBits) * BEZIER_PRECISION;
      y = byteArray.readBits(numBits) * BEZIER_PRECISION;
      keyframe.bezierOut.push({ x, y });
      x = byteArray.readBits(numBits) * BEZIER_PRECISION;
      y = byteArray.readBits(numBits) * BEZIER_PRECISION;
      keyframe.bezierIn.push({ x, y });
    }
  }
};

const readSpatialEase = <T>(_byteArray: ByteArray, _keyframes: Array<Keyframe<T>>) => {};
