import { MP4_CACHE_PATH } from './constant';
import { removeFile, touchDirectory, writeFile } from './file-utils';
import { ArrayBufferImage, type FrameData } from '@tgfx/wechat/array-buffer-image';

import type { FrameDataOptions, VideoDecoder, wx } from './interfaces';

import type {PAGPlayer} from "../pag-player";
import {PAGModule} from "../pag-module";

declare const wx: wx;
declare const setInterval: (callback: () => void, delay: number) => number;

const BUFFER_MAX_SIZE = 6;
const BUFFER_MIN_SIZE = 2;
const GET_FRAME_DATA_INTERVAL = 2; // ms

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

export interface TimeRange {
  start: number;
  end: number;
}

export class VideoReader {
  public static create(
    mp4Data: Uint8Array,
    width: number,
    height: number,
    frameRate: number,
    staticTimeRanges: TimeRange[],
  ): VideoReader {
    return new VideoReader(mp4Data, width, height, frameRate, staticTimeRanges);
  }

  public isSought = false; // Web SDK use this property to check if video is seeked
  public isPlaying = false; // Web SDK use this property to check if video is playing
  public isStart = false;
  public isStartDecode = false;
  public isSeekAndDecode: boolean | any = null;


  private readonly frameRate: number;
  private currentFrame: number;
  private mp4Path: string;
  private videoDecoder: VideoDecoder;
  private videoDecoderPromise: Promise<void> | undefined;
  private frameDataBuffers: FrameData[] = [];
  private bufferIndex = 0; // next frameData id
  private getFrameDataLooping = false;
  private getFrameDataResolve: ((frameData: FrameData) => void) | null = null;
  private getFrameDataLoopTimer: number | null = null;
  private seeking = false;
  private arrayBufferImage = new ArrayBufferImage(new ArrayBuffer(0), 0, 0);
  private player: PAGPlayer | null = null;
  private staticTimeRanges: StaticTimeRanges | null = null;
  private isFirstFrame = false;

  public constructor(
    mp4Data: Uint8Array,
    width: number,
    height: number,
    frameRate: number,
    staticTimeRanges: TimeRange[],
  ) {
    this.staticTimeRanges = new StaticTimeRanges(staticTimeRanges);
    this.frameRate = frameRate;
    this.currentFrame = -1;
    this.mp4Path = `${MP4_CACHE_PATH}${new Date().getTime()}.mp4`;
    touchDirectory(MP4_CACHE_PATH);
    writeFile(this.mp4Path, mp4Data.buffer.slice(mp4Data.byteOffset, mp4Data.byteLength + mp4Data.byteOffset));
    this.videoDecoder = wx.createVideoDecoder();
    this.videoDecoder.on('ended', () => {
      this.videoDecoder?.seek(0).then(() => {
        this.bufferIndex = 0;
      });
    });
    this.videoDecoderPromise = this.videoDecoder.start({source: this.mp4Path, mode: 1}).then((res) => {
      console.log("videoDecoder start success,res:",res);
      this.startGetFrameDataLoop();
    });
    this.linkPlayer(PAGModule.currentPlayer);
  }

  public async prepare(targetFrame: number) {
    if(this.bufferIndex === 0){
      this.isFirstFrame = true;
    }else {
      this.isFirstFrame = false;
    }
    console.log("targetFrame:",targetFrame,",this.currentFrame:",this.currentFrame);
    if (targetFrame === this.currentFrame){
      return;
    }
    if (!this.isStartDecode) {
      return;
    }
    if (this.isSeekAndDecode) {
      return;
    }
    // Wait for videoDecoder ready
    await this.videoDecoderPromise;
    if (this.frameDataBuffers.length > 0) {
      const index = this.frameDataBuffers.findIndex((frameData) => frameData.id === targetFrame);
      if (index !== -1) {
        this.frameDataBuffers = this.frameDataBuffers.slice(index);
        this.arrayBufferImage.setFrameData(await this.getFrameData());
        this.currentFrame = targetFrame;
        return;
      }
      this.frameDataBuffers = [];
    }

    if (targetFrame !== this.bufferIndex && this.isStartDecode && !this.isSeekAndDecode) {
      console.log("seek to ",Math.floor((targetFrame / this.frameRate) * 1000),",Date.now:",Date.now());
      this.frameDataBuffers = [];
      // if(this.isFirstFrame){
      //   this.isStartDecode = false;
      // }else {
      this.isSeekAndDecode = true;
      // }
      this.seeking = true;
      await this.videoDecoder.seek(Math.floor((targetFrame / this.frameRate) * 1000));
      this.seeking = false;
      this.bufferIndex = targetFrame;
    }
    this.arrayBufferImage.setFrameData(await this.getFrameData());
    this.currentFrame = targetFrame;
    return;
  }

  public getCurrentFrame() {
    return this.currentFrame;
  }

  public getVideo() {
    return this.arrayBufferImage;
  }

  public async play() {
    // Web SDK use this function to play video.
  }

  public pause() {
    // Web SDK use this function to pause video.
  }

  public stop() {
    // Web SDK use this function to stop video.
  }

  public getError(): any {
    // Web SDK use this function to get video error.
    return null;
  }

  public onDestroy() {
    if (this.player) {
      this.player.unlinkVideoReader(this);
    }
    this.clearFrameDataLoop();
    this.videoDecoder.remove();
    if (this.mp4Path) {
      removeFile(this.mp4Path);
    }
  }

  private getFrameData() {
    return new Promise<FrameData>((resolve) => {
      if (this.frameDataBuffers.length <= BUFFER_MIN_SIZE && !this.getFrameDataLooping) {
        this.startGetFrameDataLoop();
      }
      if (this.frameDataBuffers.length === 0) {
        this.getFrameDataResolve = resolve;
        return;
      }
      const res = this.frameDataBuffers.shift();
      if (!res) {
        this.getFrameDataResolve = resolve;
        return;
      }
      resolve(res);
    });
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
      if (this.getFrameDataResolve) {
        this.getFrameDataResolve(frameDataOptions2FrameData(this.bufferIndex, frameDataOptions));
        this.getFrameDataResolve = null;
      } else {
        this.frameDataBuffers.push(frameDataOptions2FrameData(this.bufferIndex, frameDataOptions));
      }
      this.bufferIndex += 1;
      if (this.isSeekAndDecode !== null) {
        this.isSeekAndDecode = false;
      }
      this.isStartDecode = true;
      console.log("this.bufferIndex:",this.bufferIndex,",frameDataOptions:",frameDataOptions,",Date.now:",Date.now());
    }
  }

  private clearFrameDataLoop() {
    if (this.getFrameDataLoopTimer) {
      clearInterval(this.getFrameDataLoopTimer);
      this.getFrameDataLoopTimer = null;
    }
    this.getFrameDataLooping = false;
  }

  private linkPlayer(player: PAGPlayer | null) {
    this.player = player;
    if (player) {
      player.linkVideoReader(this);
    }
  }

}


export class StaticTimeRanges {
  private timeRanges: TimeRange[];

  public constructor(timeRanges: TimeRange[]) {
    this.timeRanges = timeRanges;
  }

  public contains(targetFrame: number) {
    if (this.timeRanges.length === 0) return false;
    for (let timeRange of this.timeRanges) {
      if (timeRange.start <= targetFrame && targetFrame < timeRange.end) {
        return true;
      }
    }
    return false;
  }
}