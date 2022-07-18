import { TimeRange } from '../../base/time-range';
import { VideoFrame } from '../../base/video-frame';
import { VideoSequence } from '../../base/video-sequence';
import { readTime } from '../data-types';
import { readByteDataWithStartCode } from '../nalu-start-code';
import { ByteArray } from '../utils/byte-array';

export const readVideoSequence = (byteArray: ByteArray, hasAlpha: boolean): VideoSequence => {
  const videoSequence = new VideoSequence();
  videoSequence.width = byteArray.readEncodeInt32();
  videoSequence.height = byteArray.readEncodeInt32();
  videoSequence.frameRate = byteArray.readFloat32();
  if (hasAlpha) {
    videoSequence.alphaStartX = byteArray.readEncodeInt32();
    videoSequence.alphaStartY = byteArray.readEncodeInt32();
  }

  const sps = readByteDataWithStartCode(byteArray);
  const pps = readByteDataWithStartCode(byteArray);
  videoSequence.headers.push(sps, pps);

  videoSequence.frameCount = byteArray.readEncodedUint32();
  for (let i = 0; i < videoSequence.frameCount; i++) {
    const videoFrame = new VideoFrame();
    videoFrame.isKeyframe = byteArray.readBitBoolean();
    videoSequence.frames.push(videoFrame);
  }
  for (let i = 0; i < videoSequence.frameCount; i++) {
    const videoFrame = videoSequence.frames[i];
    videoFrame.frame = readTime(byteArray);
    videoFrame.fileBytes = readByteDataWithStartCode(byteArray);
  }

  if (byteArray.bytesAvailable > 0) {
    const count = byteArray.readEncodedUint32();
    for (let i = 0; i < count; i++) {
      const staticTimeRange: TimeRange = { start: 0, end: 0 };
      staticTimeRange.start = readTime(byteArray);
      staticTimeRange.end = readTime(byteArray);
      videoSequence.staticTimeRanges.push(staticTimeRange);
    }
  }

  return videoSequence;
};
