import { destroyVerify } from '../utils/decorators';
import { MP4_CACHE_PATH } from './constant';
import { touchDirectory, writeFile } from './file-utils';

import type { EmscriptenGL } from '../types';
import type { FrameDataOptions, VideoDecoder, wx } from './interfaces';

declare const wx: wx;
declare const setInterval: (callback: () => void, delay: number) => number;

const BUFFER_MAX_SIZE = 6;
const BUFFER_MIN_SIZE = 2;
const GET_FRAME_DATA_INTERVAL = 2; // ms

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

export interface TimeRange {
  start: number;
  end: number;
}

@destroyVerify
export class VideoReader {
  public static isIOS = () => {
    // need't to check platform on wechat miniprogram
    return false;
  };

  private readonly frameRate: number;
  private currentFrame: number;
  private mp4Path: string;
  private videoDecoder: VideoDecoder;
  private videoDecoderPromise: Promise<void> | undefined;
  private frameData: FrameData | null = null;
  private frameDataBuffers: FrameData[] = [];
  private bufferIndex = 0; // next frameData id
  private getFrameDataLooping = false;
  private getFrameDataResolve: ((frameData: FrameData) => void) | null = null;
  private getFrameDataLoopTimer: number | null = null;
  private seeking = false;

  public constructor(
    mp4Data: Uint8Array,
    width: number,
    height: number,
    frameRate: number,
    staticTimeRanges: TimeRange[],
  ) {
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
    this.videoDecoderPromise = this.videoDecoder.start({ source: this.mp4Path, mode: 1 }).then(() => {
      this.startGetFrameDataLoop();
    });
  }

  public async prepare(targetFrame: number) {
    if (targetFrame === this.currentFrame) {
      return true;
    }
    // Wait for videoDecoder ready
    await this.videoDecoderPromise;
    if (this.frameDataBuffers.length > 0) {
      const index = this.frameDataBuffers.findIndex((frameData) => frameData.id === targetFrame);
      if (index !== -1) {
        this.frameDataBuffers = this.frameDataBuffers.slice(index);
        this.frameData = await this.getFrameData();
        this.currentFrame = targetFrame;
        return true;
      }
      this.frameDataBuffers = [];
    }

    if (targetFrame !== this.bufferIndex) {
      this.seeking = true;
      await this.videoDecoder.seek(Math.floor((targetFrame / this.frameRate) * 1000));
      this.seeking = false;
    }
    this.frameData = await this.getFrameData();
    this.currentFrame = targetFrame;
    return true;
  }

  public renderToTexture(GL: EmscriptenGL, textureID: number) {
    const gl = GL.currentContext!.GLctx;
    gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      this.frameData!.width,
      this.frameData!.height,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      new Uint8Array(this.frameData!.data),
    );
  }

  public onDestroy() {
    this.videoDecoder.remove();
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
