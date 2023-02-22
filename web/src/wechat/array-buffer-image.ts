import type { FrameData } from './video-reader';

export class ArrayBufferImage {
  public buffer: ArrayBuffer;
  public width: number;
  public height: number;
  public constructor(buffer: ArrayBuffer, width: number, height: number) {
    this.buffer = buffer;
    this.width = width;
    this.height = height;
  }

  public setFrameData(frameData: FrameData) {
    this.buffer = frameData.data;
    this.width = frameData.width;
    this.height = frameData.height;
  }
}
