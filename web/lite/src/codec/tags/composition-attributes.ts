import { Composition } from '../../base/composition';
import { readColor, readTime } from '../data-types';
import { ByteArray } from '../utils/byte-array';

export const readCompositionAttributes = (byteArray: ByteArray, composition: Composition) => {
  composition.width = byteArray.readEncodeInt32();
  composition.height = byteArray.readEncodeInt32();
  composition.duration = readTime(byteArray);
  composition.frameRate = byteArray.readFloat32();
  composition.backgroundColor = readColor(byteArray);
};
