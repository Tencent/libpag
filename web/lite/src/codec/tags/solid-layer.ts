import { SolidLayer } from '../../base/solid-layer';
import { readColor } from '../data-types';
import { ByteArray } from '../utils/byte-array';

export function readSolidColor(byteArray: ByteArray, layer: SolidLayer) {
  layer.solidColor = readColor(byteArray);
  layer.width = byteArray.readEncodeInt32();
  layer.height = byteArray.readEncodeInt32();
}
