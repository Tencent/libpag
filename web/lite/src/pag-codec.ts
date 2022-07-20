import { ByteArray } from './codec/utils/byte-array';
import { readTags } from './codec/tags/tag-header';
import { readTagsOfFile } from './codec/tags/file-tags';
import { Composition } from './base/composition';
import { ImageBytes } from './base/image-bytes';
import { CompositionType, TagCode } from './codec/types';
import { VectorComposition } from './base/vector-composition';
import { LayerType } from './base/layer';
import { PreComposeLayer } from './base/pre-compose-layer';

export const verifyAndMake = (compositions: Array<Composition>) => {
  let success: boolean = compositions.length > 0;
  for (const composition of compositions) {
    if (!composition || !composition.verify()) {
      success = false;
      break;
    }
  }
  if (!success) {
    throw new Error('Verify composition failed!');
  }
};

/**
 * 将Layer里面的compositionID标识换成真正的Composition
 */
export function installReference(compositions: Array<Composition>) {
  if (!compositions || compositions.length === 0) return;
  const compositionMap = new Map();
  compositions.forEach((composition) => {
    if (composition) {
      compositionMap.set(composition.id, composition);
    }
  });
  compositions.forEach((composition) => {
    if (composition && composition.type() === CompositionType.Vector) {
      const vectorComposition = composition as VectorComposition;
      if (vectorComposition.layers && vectorComposition.layers.length > 0) {
        vectorComposition.layers.forEach((layer) => {
          layer.containingComposition = vectorComposition;
          const preComposeLayer = layer as PreComposeLayer;
          if (preComposeLayer.type() === LayerType.PreCompose && preComposeLayer.composition) {
            const res = compositionMap.get(preComposeLayer.composition.id);
            if (res) {
              preComposeLayer.composition = res;
            }
          }
        });
      }
    }
  });
}

export class PAGCodec {
  public static maxSupportedTagLevel(): number {
    return TagCode.Count - 1;
  }
}

/**
 * Decode PAG File from bytes
 */
export const decode = (byteArray: ByteArray) => {
  const bodyByteArray: ByteArray = readBodyBytes(byteArray);
  const { context } = bodyByteArray;
  readTags(bodyByteArray, context, readTagsOfFile);
  installReference(context.compositions);
  const compositions = context.releaseCompositions();
  verifyAndMake(compositions);
  return { compositions, tagLevel: context.tagLevel };
};

export const readBodyBytes = (byteArray: ByteArray): ByteArray => {
  if (byteArray.length < 11) throw new Error('PAG file is invalid!');
  const P: number = byteArray.readInt8();
  const A: number = byteArray.readInt8();
  const G: number = byteArray.readInt8();
  if (P !== 80 || A !== 65 || G !== 71) throw new Error('invalid PAG header!');
  byteArray.readInt8(); // version
  byteArray.readUint32(); // bodyLength
  byteArray.readInt8(); // compression
  return byteArray.readBytes();
};
