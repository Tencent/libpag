import { ByteData } from './byte-data';
import { Sequence } from './sequence';
import { TimeRange } from './time-range';
import { verifyFailed } from './utils/verify';
import { VideoFrame } from './video-frame';

export class VideoSequence extends Sequence {
  public alphaStartX = 0;
  public alphaStartY = 0;
  public frames: Array<VideoFrame> = [];
  public headers: Array<ByteData> = [];
  public staticTimeRanges: Array<TimeRange> = [];

  public verify(): boolean {
    if (!super.verify() || this.frames.length <= 0) {
      verifyFailed();
      return false;
    }
    for (const frame of this.frames) {
      if (!frame || !frame.fileBytes) {
        verifyFailed();
        return false;
      }
    }
    for (const header of this.headers) {
      if (!header) {
        verifyFailed();
        return false;
      }
    }
    return true;
  }
  // The exact total width and height of the picture were not recorded when the video sequence frame
  // was exported，You need to do the calculation yourself with width and alphaStartX，
  // If an odd size is encountered, the exporter plugin automatically increments by one，
  // This matches the rules for exporter plugin。
  public getVideoWidth() {
    let videoWidth = this.alphaStartX + this.width;
    if (videoWidth % 2 === 1) {
      videoWidth += 1;
    }
    return videoWidth;
  }

  public getVideoHeight() {
    let videoHeight = this.alphaStartY + this.height;
    if (videoHeight % 2 === 1) {
      videoHeight += 1;
    }
    return videoHeight;
  }
}
