import { EffectType } from '../../base/effects/effect';
import { FillEffect } from '../../base/effects/fill-effect';
import { StrokeEffect } from '../../base/effects/stroke-effect';
import { Layer, LayerType, TrackMatteType } from '../../base/layer';
import { Mask } from '../../base/mask';
import { TextLayer } from '../../base/text-layer';
import { VectorComposition } from '../../base/vector-composition';
import { TagCode } from '../types';
import { ByteArray } from '../utils/byte-array';
import { readCompositionAttributes } from './composition-attributes';
import { readLayer } from './layer-tag';
import { readTags } from './tag-header';

export const readVectorComposition = (byteArray: ByteArray): VectorComposition => {
  const composition = new VectorComposition();
  composition.id = byteArray.readEncodedUint32();
  readTags(byteArray, composition, readTagsOfVectorComposition);
  installArrayLayerReference(composition.layers);
  return composition;
};

export const readTagsOfVectorComposition = (byteArray: ByteArray, code: TagCode, composition: VectorComposition) => {
  switch (code) {
    case TagCode.CompositionAttributes:
      readCompositionAttributes(byteArray, composition);
      break;
    case TagCode.LayerBlock:
      composition.layers.push(readLayer(byteArray));
      break;
    default:
      break;
  }
};

export const installArrayLayerReference = (layers: Array<Layer>) => {
  if (layers && layers.length === 0) {
    return;
  }
  const layerMap = new Map();
  for (const layer of layers) {
    if (!layer) {
      continue;
    }
    installLayerReference(layer);
    layerMap.set(layer.id, layer);
  }

  let index = 0;
  for (const layer of layers) {
    if (!layer) {
      continue;
    }
    if (layer.parent !== undefined) {
      const ID = layer.parent.id;
      const result = layerMap.get(ID);
      if (result !== undefined) {
        layer.parent = result;
      }
    }
    if (index > 0 && hasTrackMatte(layer.trackMatteType)) {
      layer.trackMatteLayer = layers[index - 1];
    }
    if (layer.effects !== undefined && layer.effects.length > 0) {
      for (const effect of layer.effects) {
        if (!effect) {
          continue;
        }
        if (effect.type() === EffectType.DisplacementMap) {
          // let displacementMapEffect = <DisplacementMapEffect> effect;
          // if (displacementMapEffect.displacementMapLayer != undefined) {
          //     let ID = displacementMapEffect.displacementMapLayer.id;
          //     let result = layerMap.get(ID);
          //     if (result != undefined) {
          //         displacementMapEffect.displacementMapLayer = result;
          //     }
          // }
        }
      }
    }
    index += 1;
  }
};

/**
 * 将Layer里面的MaskID标识换成真正的Mask
 * @param layer
 */
const installLayerReference = (layer: Layer) => {
  if (!layer || !layer.masks || layer.masks.length === 0) return;
  const maskMap = new Map();
  for (const mask of layer.masks) {
    if (!mask) {
      continue;
    }
    maskMap.set(mask.id, mask);
  }

  layer.effects?.forEach((effect) => {
    if (!effect) return;
    if (effect.maskReferences !== undefined && effect.maskReferences.length > 0) {
      const maskReferences = new Array<Mask>();
      effect.maskReferences.forEach((mask) => {
        const ID = mask.id;
        const result = maskMap.get(ID);
        if (result !== undefined) {
          maskReferences.push(result);
        }
      });
      effect.maskReferences = maskReferences;
    }
    switch (effect.type()) {
      case EffectType.Fill:
        if ((effect as FillEffect).fillMask !== undefined) {
          const ID = (effect as FillEffect).fillMask!.id;
          const result = maskMap.get(ID);
          if (result !== undefined) {
            (effect as FillEffect).fillMask = result;
          }
        }
        break;
      case EffectType.Stroke: {
        const strokeEffect = effect as StrokeEffect;
        if (strokeEffect.path !== undefined) {
          const ID = strokeEffect.path.id;
          const result = maskMap.get(ID);
          if (result !== undefined) {
            strokeEffect.path = result;
          }
        }
        break;
      }
      default:
        break;
    }
  });

  if (layer.type() === LayerType.Text) {
    const { pathOption } = layer as TextLayer;
    if (pathOption?.path) {
      const ID = pathOption.path.id;
      const result = maskMap.get(ID);
      if (result !== undefined) {
        pathOption.path = result;
      }
    }
  }
};

export const hasTrackMatte = (type: TrackMatteType): boolean => {
  switch (type) {
    case TrackMatteType.Alpha:
    case TrackMatteType.AlphaInverted:
      return true;
    default:
      return false;
  }
};
