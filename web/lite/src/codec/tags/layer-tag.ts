import { Layer, LayerType } from '../../base/layer';
import { ZERO_POINT } from '../../base/point';
import { PreComposeLayer } from '../../base/pre-compose-layer';
import { ShapeLayer } from '../../base/shape-layer';
import { SolidLayer } from '../../base/solid-layer';
import { Transform2D } from '../../base/transform-2d';
import { UnDefinedLayer } from '../../base/un-defined-layer';
import { TagCode } from '../types';
import { readTagBlock } from '../attribute-helper';
import { ByteArray } from '../utils/byte-array';
import { readSolidColor } from './solid-layer';
import {
  readBlockConfigOfLayerAttributes,
  readBlockConfigOfLayerAttributesV2,
  readBlockConfigOfTransform2D,
} from './tag-attributes';
import { readTags } from './tag-header';
import { readCompositionReference } from './read-composition-reference';
import { Property } from '../../base/property';

export const readLayer = (byteArray: ByteArray): Layer => {
  const layerType: LayerType = byteArray.readUint8();
  let layer: Layer;
  switch (layerType) {
    case LayerType.undefined:
      layer = new UnDefinedLayer();
      break;
    case LayerType.Solid:
      layer = new SolidLayer();
      break;
    case LayerType.Shape:
      layer = new ShapeLayer();
      break;
    case LayerType.PreCompose:
      layer = new PreComposeLayer();
      break;
    default:
      layer = new Layer();
      break;
  }
  layer.id = byteArray.readEncodedUint32();
  readTags(byteArray, layer, readTagsOfLayer);
  return layer;
};

export const readTagsOfLayer = (byteArray: ByteArray, code: TagCode, layer: Layer) => {
  switch (code) {
    case TagCode.LayerAttributes:
      readTagBlock(byteArray, layer, readBlockConfigOfLayerAttributes);
      if (layer.duration <= 0) layer.duration = 1;
      break;
    case TagCode.LayerAttributesV2:
      readTagBlock(byteArray, layer, readBlockConfigOfLayerAttributesV2);
      if (layer.duration <= 0) layer.duration = 1;
      break;
    case TagCode.Transform2D:
      layer.transform = new Transform2D();
      readTagBlock(byteArray, layer.transform, readBlockConfigOfTransform2D);
      // hasPosition || (!hasXPosition && !hasXPosition)
      if (
        layer.transform.position.animatable() ||
        layer.transform.position.value !== ZERO_POINT ||
        (!(layer.transform.xPosition.animatable() || layer.transform.xPosition.value !== 0) &&
          !(layer.transform.yPosition.animatable() || layer.transform.yPosition.value !== 0))
      ) {
        layer.transform.xPosition = new Property(0);
        layer.transform.yPosition = new Property(0);
      } else {
        layer.transform.position = new Property(ZERO_POINT);
      }
      break;
    case TagCode.SolidColor:
      if (layer.type() === LayerType.Solid) {
        readSolidColor(byteArray, layer as SolidLayer);
      }
      break;
    case TagCode.CompositionReference:
      if (layer.type() === LayerType.PreCompose) {
        readCompositionReference(byteArray, layer as PreComposeLayer);
      }
      break;
    default:
      break;
  }
};
