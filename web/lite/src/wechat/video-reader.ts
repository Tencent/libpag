import { VideoSequence } from '../base/video-sequence';
import { destroyVerify } from '../decorators';
import { coverToMp4 } from '../generator/mp4-box-helper';
import { VideoReader as Reader } from '../view/video-reader';
import { removeFile, touchDirectory, writeFile } from './file-utils';

import type { VideoDecoder, wx } from './types';

type K = keyof HTMLVideoElementEventMap;

declare const wx: wx;

const MP4_CACHE_PATH = `${wx.env.USER_DATA_PATH}/pag/`;

@destroyVerify
export class VideoReader extends Reader {
  public videoDecoder: VideoDecoder | undefined;

  private mp4Path: string | undefined;

  public constructor(videoSequence: VideoSequence) {
    super(videoSequence);
  }

  public getVideoElement(): HTMLVideoElement {
    throw new Error('WeChat mini program does not support video element as decoder!');
  }

  public progress(): number {
    return 0;
  }

  public duration(): number {
    return 0;
  }

  public currentTime(): number {
    return 0;
  }

  public start(): Promise<void> {
    throw new Error('VideoDecoder will be ready to start when loaded, no restart required!');
  }

  public pause() {
    // NOP
  }

  public seek(time: number) {
    return this.videoDecoder?.seek(time) as Promise<void>;
  }

  public getFrameData() {
    return this.videoDecoder?.getFrameData();
  }

  public addListener(event: K, handler: (this: HTMLVideoElement, ev: HTMLVideoElementEventMap[K]) => void) {
    const ev = event as 'start' | 'stop' | 'seek' | 'bufferchange' | 'ended';
    this.videoDecoder?.on(ev, handler);
  }

  public removeAllListeners() {
    // NOP
  }

  public destroy() {
    this.videoDecoder?.remove();
    this.videoDecoder = undefined;
    if (this.mp4Path) {
      removeFile(this.mp4Path);
    }
    this.destroyed = true;
  }

  protected override load(videoSequence: VideoSequence) {
    this.mp4Path = `${MP4_CACHE_PATH}${Date.now()}.mp4`;
    touchDirectory(MP4_CACHE_PATH);
    const mp4Data = coverToMp4(videoSequence);
    writeFile(this.mp4Path, mp4Data.buffer);
    this.videoDecoder = wx.createVideoDecoder();
    this.videoDecoder.start({ source: this.mp4Path, mode: 0 }); // prepare
  }
}
