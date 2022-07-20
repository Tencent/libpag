import { TrackMatteType } from '../../base/layer';
import { Point, ZERO_POINT } from '../../base/point';
import { DefaultRatio } from '../../base/ratio';
import { BlendMode, OPAQUE, ZERO_TIME } from '../../constant';
import { TagCode } from '../types';
import { AttributeType, BlockConfig } from '../attribute-helper';
import {
  BOOLAttributeConfig,
  FloatAttributeConfig,
  LayerAttributeConfig,
  PointAttributeConfig,
  RatioAttributeConfig,
  StringAttributeConfig,
  TimeAttributeConfig,
  Uint8AttributeConfig,
} from '../attributes';

export const readBlockConfigOfLayerAttributes: BlockConfig = {
  tagCode: TagCode.LayerAttributes,
  configs: [
    new BOOLAttributeConfig('isActive', AttributeType.BitFlag, true),
    new BOOLAttributeConfig('autoOrientation', AttributeType.BitFlag, false),
    new LayerAttributeConfig('parent', AttributeType.Value, undefined),
    new RatioAttributeConfig('stretch', AttributeType.Value, DefaultRatio),
    new TimeAttributeConfig('startTime', AttributeType.Value, ZERO_TIME),
    new Uint8AttributeConfig('blendMode', AttributeType.Value, BlendMode.Normal),
    new Uint8AttributeConfig('trackMatteType', AttributeType.Value, TrackMatteType.None),
    new FloatAttributeConfig('timeRemap', AttributeType.SimpleProperty, 0),
    new TimeAttributeConfig('duration', AttributeType.FixedValue, ZERO_TIME),
  ],
};

export const readBlockConfigOfLayerAttributesV2: BlockConfig = {
  tagCode: TagCode.LayerAttributesV2,
  configs: [
    new BOOLAttributeConfig('isActive', AttributeType.BitFlag, true),
    new BOOLAttributeConfig('autoOrientation', AttributeType.BitFlag, false),
    new LayerAttributeConfig('parent', AttributeType.Value, undefined),
    new RatioAttributeConfig('stretch', AttributeType.Value, DefaultRatio),
    new TimeAttributeConfig('startTime', AttributeType.Value, ZERO_TIME),
    new Uint8AttributeConfig('blendMode', AttributeType.Value, BlendMode.Normal),
    new Uint8AttributeConfig('trackMatteType', AttributeType.Value, TrackMatteType.None),
    new FloatAttributeConfig('timeRemap', AttributeType.SimpleProperty, 0),
    new TimeAttributeConfig('duration', AttributeType.FixedValue, ZERO_TIME),
    new StringAttributeConfig('name', AttributeType.Value, ''),
  ],
};

export const readBlockConfigOfTransform2D: BlockConfig = {
  tagCode: TagCode.Transform2D,
  configs: [
    new PointAttributeConfig('anchorPoint', AttributeType.SpatialProperty, ZERO_POINT),
    new PointAttributeConfig('position', AttributeType.SpatialProperty, ZERO_POINT),
    new FloatAttributeConfig('xPosition', AttributeType.SimpleProperty, 0),
    new FloatAttributeConfig('yPosition', AttributeType.SimpleProperty, 0),
    new PointAttributeConfig('scale', AttributeType.MultiDimensionProperty, new Point(1, 1)),
    new FloatAttributeConfig('rotation', AttributeType.SimpleProperty, 0),
    new Uint8AttributeConfig('opacity', AttributeType.SimpleProperty, OPAQUE),
  ],
};

export const readBlockConfigOfMask: BlockConfig = {
  tagCode: TagCode.MaskBlock,
  configs: [],
};
