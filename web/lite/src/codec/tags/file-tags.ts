import { readVideoComposition } from './video-composition-tag';
import { readVectorComposition } from './vector-composition-tag';
import { TagCode } from '../types';

import type { Context } from '../context';
import type { ByteArray } from '../utils/byte-array';

export function readTagsOfFile(byteArray: ByteArray, code: TagCode, context: Context): void {
  switch (code) {
    case TagCode.VectorCompositionBlock:
      context.compositions.push(readVectorComposition(byteArray));
      break;
    case TagCode.VideoCompositionBlock:
      context.compositions.push(readVideoComposition(byteArray));
      break;
    default:
      break;
  }
}
