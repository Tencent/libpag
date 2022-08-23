import { Clock } from '../base/utils/clock';
import { VideoSequence } from '../base/video-sequence';
import { destroyVerify } from '../decorators';
import { coverToMp4 } from '../generator/mp4-box-helper';
import { VideoReader as Reader } from '../view/video-reader';
import { removeFile, touchDirectory, writeFile } from './file-utils';

import type { FrameDataOptions, VideoDecoder, wx } from './types';

type K = keyof HTMLVideoElementEventMap;

declare const wx: wx;
declare const setTimeout: (callback: () => void, delay: number) => number;

const MP4_CACHE_PATH = `${wx.env.USER_DATA_PATH}/pag/`;
const BUFFER_MAX_SIZE = 6;
const BUFFER_MIN_SIZE = 2;

export interface FrameData {
  id: number;
  data: ArrayBuffer;
  width: number;
  height: number;
}

const frameDataOptions2FrameData = (id: number, options: FrameDataOptions): FrameData => {
  const data = new ArrayBuffer(options.data.byteLength);
  new Uint8Array(data).set(new Uint8Array(options.data));
  return {
    id: id,
    data: data,
    width: options.width,
    height: options.height,
  };
};

@destroyVerify
export class VideoReader extends Reader {
  public static create(videoSequence: VideoSequence) {
    const videoReader = new VideoReader(videoSequence);
    const debugData = videoReader.load(videoSequence);
    return { videoReader: videoReader, debugData: debugData };
  }

  public videoDecoder: VideoDecoder | undefined;

  private started = false;
  private mp4Path: string | undefined;
  private loadedPromise: Promise<void> | undefined;
  private frameDataBuffer: FrameData[] = [];
  private getFrameDataLooping = false;
  private getFrameDataCallback: ((frameData: FrameData) => void) | null = null;
  private getFrameDataLoopTimer: number | null = null;
  private currentFrame = 0;

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

  public async start(): Promise<void> {
    if (this.started) return;
    this.started = true;
    await this.loadedPromise;
    this.startGetFrameDataLoop();
  }

  public pause() {
    // NOP
  }

  public seek(time: number) {
    return this.videoDecoder?.seek(time) as Promise<void>;
  }

  public getFrameData(callback: (frameData: FrameData) => void) {
    if (this.frameDataBuffer.length === 0) {
      this.getFrameDataCallback = callback;
      return;
    }
    const res = this.frameDataBuffer.shift();
    if (!res) {
      this.getFrameDataCallback = callback;
      return;
    }
    if (this.frameDataBuffer.length <= BUFFER_MIN_SIZE && !this.getFrameDataLooping) {
      this.startGetFrameDataLoop();
    }
    callback(res);
  }

  public addListener(event: K, handler: (this: HTMLVideoElement, ev: HTMLVideoElementEventMap[K]) => void) {
    const ev = event as 'start' | 'stop' | 'seek' | 'bufferchange' | 'ended';
    this.videoDecoder?.on(ev, handler);
  }

  public removeAllListeners() {
    // NOP
  }

  public destroy() {
    if (this.getFrameDataLoopTimer) {
      clearTimeout(this.getFrameDataLoopTimer);
      this.getFrameDataLoopTimer = null;
    }
    if (this.getFrameDataCallback) this.getFrameDataCallback = null;
    this.videoDecoder?.remove();
    this.videoDecoder = undefined;
    if (this.mp4Path) {
      removeFile(this.mp4Path);
    }
    this.destroyed = true;
  }

  public clearCallback() {
    this.getFrameDataCallback = null;
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
    this.loadedPromise = this.videoDecoder.start({ source: this.mp4Path, mode: 0 }); // prepare
    this.videoDecoder.on('ended', () => {
      console.log('ended', this.frameDataBuffer.length, this.currentFrame);
    });
    return {
      createDir: clock.measure('', 'createDir'),
      coverMP4: clock.measure('createDir', 'coverMP4'),
      writeFile: clock.measure('coverMP4', 'writeFile'),
      createDecoder: clock.measure('writeFile', 'createDecoder'),
    };
  }

  private startGetFrameDataLoop() {
    this.getFrameDataLooping = true;
    this.getFrameDataLoop();
  }

  private getFrameDataLoop() {
    if (!this.videoDecoder) throw new Error('VideoDecoder is not ready!');
    if (this.frameDataBuffer.length >= BUFFER_MAX_SIZE) {
      this.getFrameDataLooping = false;
      return;
    }
    const frameDataOptions = this.videoDecoder.getFrameData();
    if (frameDataOptions !== null) {
      if (this.getFrameDataCallback) {
        this.getFrameDataCallback(frameDataOptions2FrameData(this.currentFrame, frameDataOptions));
        this.getFrameDataCallback = null;
      } else {
        this.frameDataBuffer.push(frameDataOptions2FrameData(this.currentFrame, frameDataOptions));
      }
      this.currentFrame += 1;
    }
    this.getFrameDataLoopTimer = setTimeout(() => this.getFrameDataLoop(), 2);
  }
}
