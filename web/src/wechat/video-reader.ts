import { destroyVerify } from '../utils/decorators';
import { MP4_CACHE_PATH } from './constant';
import { touchDirectory, writeFile } from './file-utils';

import type { EmscriptenGL } from '../types';
import type { FrameDataOptions, VideoDecoder, wx } from './interfaces';

declare const wx: wx;

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
  private frameDataOptions: FrameDataOptions | null = null;

  public constructor(mp4Data: Uint8Array, frameRate: number, staticTimeRanges: TimeRange[]) {
    this.frameRate = frameRate;
    this.currentFrame = -1;
    this.mp4Path = `${MP4_CACHE_PATH}${new Date().getTime()}.mp4`;
    touchDirectory(MP4_CACHE_PATH);
    writeFile(this.mp4Path, mp4Data.buffer.slice(mp4Data.byteOffset, mp4Data.byteLength + mp4Data.byteOffset));
    this.videoDecoder = wx.createVideoDecoder();
    this.videoDecoderPromise = this.videoDecoder.start({ source: this.mp4Path, mode: 1 });
  }

  public async prepare(targetFrame: number) {
    if (targetFrame === this.currentFrame) {
      return true;
    } else {
      await this.videoDecoderPromise;
      if (targetFrame !== this.currentFrame + 1) {
        await this.videoDecoder.seek(Math.floor((targetFrame / this.frameRate) * 1000));
      }
      this.frameDataOptions = await this.getFrameData();
      this.currentFrame = targetFrame;
      return true;
    }
  }

  public renderToTexture(GL: EmscriptenGL, textureID: number) {
    const gl = GL.currentContext!.GLctx;
    gl.bindTexture(gl.TEXTURE_2D, GL.textures[textureID]);
    gl.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      this.frameDataOptions!.width,
      this.frameDataOptions!.height,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      new Uint8Array(this.frameDataOptions!.data),
    );
  }

  public onDestroy() {
    this.videoDecoder.remove();
  }

  private getFrameData() {
    return new Promise<FrameDataOptions>((resolve) => {
      const loop = () => {
        const frameData = this.videoDecoder.getFrameData();
        if (frameData !== null) {
          resolve(frameData);
          return;
        }
        setTimeout(() => {
          loop();
        }, 1);
      };
      loop();
    });
  }
}
