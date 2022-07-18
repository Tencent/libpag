import { ByteArray } from '../utils/byte-array';
import { PreComposeLayer } from '../../base/pre-compose-layer';
import { Composition } from '../../base/composition';
import { readTime } from '../data-types';

export function readCompositionReference(byteArray: ByteArray, layer: PreComposeLayer) {
  const id = byteArray.readEncodedUint32();
  if (id > 0) {
    layer.composition = new Composition();
    layer.composition.id = id;
  }
  layer.compositionStartTime = readTime(byteArray);
}
