import { Clock } from '../base/utils/clock';
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
  public static create(videoSequence: VideoSequence) {
    const videoReader = new VideoReader();
    const debugData = videoReader.load(videoSequence);
    return { videoReader: videoReader, debugData: debugData };
  }

  public videoDecoder: VideoDecoder | undefined;

  private mp4Path: string | undefined;

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
    const clock = new Clock();
    this.mp4Path = `${MP4_CACHE_PATH}${Date.now()}.mp4`;
    touchDirectory(MP4_CACHE_PATH);
    clock.mark('createDir');
    const mp4Data = coverToMp4(videoSequence);
    clock.mark('coverMP4');
    writeFile(this.mp4Path, mp4Data.buffer);
    clock.mark('writeFile');
    this.videoDecoder = wx.createVideoDecoder();
    clock.mark('createDecoder');
    this.videoDecoder.start({ source: this.mp4Path, mode: 0 }); // prepare
    return {
      createDir: clock.measure('', 'createDir'),
      coverMP4: clock.measure('createDir', 'coverMP4'),
      writeFile: clock.measure('coverMP4', 'writeFile'),
      createDecoder: clock.measure('writeFile', 'createDecoder'),
    };
  }
}
