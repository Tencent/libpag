import { VideoComposition } from '../../base/video-composition';
import { TagCode } from '../types';
import { ByteArray } from '../utils/byte-array';
import { readCompositionAttributes } from './composition-attributes';
import { readTags } from './tag-header';
import { readVideoSequence } from './video-sequence-tag';

export const readVideoComposition = (byteArray: ByteArray): VideoComposition => {
  const composition = new VideoComposition();
  composition.id = byteArray.readEncodedUint32();
  composition.hasAlpha = byteArray.readBoolean();
  const parameter = { composition, hasAlpha: composition.hasAlpha };
  readTags(byteArray, parameter, ReadTagsOfVideoComposition);
  return composition;
};

export const ReadTagsOfVideoComposition = (
  byteArray: ByteArray,
  code: TagCode,
  parameter: { composition: VideoComposition; hasAlpha: boolean },
) => {
  const { composition } = parameter;
  switch (code) {
    case TagCode.CompositionAttributes:
      readCompositionAttributes(byteArray, composition);
      break;
    case TagCode.VideoSequence: {
      const sequence = readVideoSequence(byteArray, parameter.hasAlpha);
      sequence.composition = composition;
      composition.sequences.push(sequence);
      break;
    }
    default:
      break;
  }
};
