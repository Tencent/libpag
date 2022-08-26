import { MP4_CACHE_PATH } from './constant';
import { Clock } from '../base/utils/clock';
import { destroyVerify } from '../decorators';
import { coverToMp4 } from '../generator/mp4-box-helper';
import { VideoReader as Reader } from '../view/video-reader';
import { removeFile, touchDirectory, writeFile } from './file-utils';

import type { VideoSequence } from '../base/video-sequence';
import type { FrameDataOptions, VideoDecoder, wx } from './types';

type K = keyof HTMLVideoElementEventMap;

declare const wx: wx;
declare const setInterval: (callback: () => void, delay: number) => number;

const BUFFER_MAX_SIZE = 6;
const BUFFER_MIN_SIZE = 2;
const GET_FRAME_DATA_INTERVAL = 10; // ms

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
  private frameDataBuffers: FrameData[] = [];
  private getFrameDataLooping = false;
  private getFrameDataCallback: ((frameData: FrameData) => void) | null = null;
  private getFrameDataLoopTimer: number | null = null;
  private currentFrame = 0;
  private seeking = false;

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

  public async seek(time: number) {
    const targetFrame = Math.floor(time * this.frameRate);
    // check buffers
    if (this.frameDataBuffers.length > 0) {
      const index = this.frameDataBuffers.findIndex((f) => f.id === targetFrame);
      // find in buffers
      if (index !== -1) {
        this.frameDataBuffers = this.frameDataBuffers.slice(index);
        return;
      }
      // clear buffers
      this.frameDataBuffers = [];
    }
    // current frame is the target frame
    if (targetFrame === this.currentFrame) return;
    // seek
    this.seeking = true;
    await this.videoDecoder?.seek(time);
    this.seeking = false;
    this.currentFrame = targetFrame;
  }

  public getFrameData(callback: (frameData: FrameData) => void) {
    if (this.frameDataBuffers.length <= BUFFER_MIN_SIZE && !this.getFrameDataLooping) {
      this.startGetFrameDataLoop();
    }
    if (this.frameDataBuffers.length === 0) {
      this.getFrameDataCallback = callback;
      return;
    }
    const res = this.frameDataBuffers.shift();
    if (!res) {
      this.getFrameDataCallback = callback;
      return;
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
    this.clearFrameDataLoop();
    this.clearCallback();
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
      this.videoDecoder?.seek(0).then(() => {
        this.currentFrame = 0;
      });
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
    this.getFrameDataLoopTimer = setInterval(() => {
      this.getFrameDataLoop();
    }, GET_FRAME_DATA_INTERVAL);
  }

  private getFrameDataLoop() {
    if (this.seeking) return;

    if (!this.videoDecoder) {
      this.clearFrameDataLoop();
      throw new Error('VideoDecoder is not ready!');
    }

    if (this.frameDataBuffers.length >= BUFFER_MAX_SIZE) {
      this.getFrameDataLooping = false;
      this.clearFrameDataLoop();
      return;
    }

    const frameDataOptions = this.videoDecoder.getFrameData();
    if (frameDataOptions !== null) {
      if (this.getFrameDataCallback) {
        this.getFrameDataCallback(frameDataOptions2FrameData(this.currentFrame, frameDataOptions));
        this.getFrameDataCallback = null;
      } else {
        this.frameDataBuffers.push(frameDataOptions2FrameData(this.currentFrame, frameDataOptions));
      }
      this.currentFrame += 1;
    }
  }

  private clearFrameDataLoop() {
    if (this.getFrameDataLoopTimer) {
      clearInterval(this.getFrameDataLoopTimer);
      this.getFrameDataLoopTimer = null;
    }
    this.getFrameDataLooping = false;
  }
}
