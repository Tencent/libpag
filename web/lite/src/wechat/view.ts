import { Context } from '../view/context';
import { destroyVerify } from '../decorators';
import { removeFile, touchDirectory, writeFile } from './file-utils';
import { coverToMp4 } from '../generator/mp4-box-helper';
import { RenderingMode, EventName } from '../types';

import type { PAGFile } from '../pag-file';
import type { FrameDataOptions, VideoDecoder, wx } from './types';

declare const wx: wx;

declare const setTimeout: (callback: () => void, delay: number) => number;

const MP4_CACHE_PATH = `${wx.env.USER_DATA_PATH}/pag/`;

@destroyVerify
export class View extends Context {
  private mp4Path: string;
  private videoDecoder: VideoDecoder;
  private flushBaseTime;
  private currentFrame = 0;

  public constructor(pagFile: PAGFile, canvas: HTMLCanvasElement) {
    super(pagFile, canvas, { renderingMode: RenderingMode.WebGL });
    this.mp4Path = `${MP4_CACHE_PATH}${Date.now()}.mp4`;
    touchDirectory(MP4_CACHE_PATH);
    const mp4Data = coverToMp4(this.videoSequence);
    writeFile(this.mp4Path, mp4Data.buffer);
    this.videoDecoder = wx.createVideoDecoder();
    this.flushBaseTime = Math.floor(1000 / this.videoSequence.frameRate);
    this.videoDecoder.on('ended', () => {
      this.repeat();
    });
    this.videoDecoder.start({ source: this.mp4Path, mode: 0 });
  }

  /**
   * 开始播放
   */
  public async play() {
    if (this.playing) return;
    this.playing = true;
    this.flushLoop();
    if (this.getProgress() === 0) {
      this.eventManager.emit(EventName.onAnimationStart);
    }
    this.eventManager.emit(EventName.onAnimationPlay);
  }
  /**
   * 暂停播放
   */
  public pause() {
    if (!this.playing) return;
    this.clearRenderTimer();
    this.playing = false;
    this.eventManager.emit(EventName.onAnimationPause);
  }
  /**
   * 停止播放
   */
  public stop() {
    this.clearRenderTimer();
    this.videoDecoder.seek(0);
    this.clearRender();
    this.playing = false;
    this.eventManager.emit(EventName.onAnimationCancel);
  }
  /**
   * 销毁播放实例
   */
  public destroy() {
    this.clearRenderTimer();
    this.videoDecoder.remove();
    this.clearRender();
    this.canvas = null;
    removeFile(this.mp4Path);
    this.destroyed = true;
  }
  /**
   * 返回当前播放进度位置，取值范围为 0.0 到 1.0。
   */
  public getProgress() {
    return this.currentFrame / this.videoSequence.frameCount;
  }
  /**
   * 设置播放进度位置，取值范围为 0.0 到 1.0。
   */
  public async setProgress(progress: number) {
    if (progress < 0 || progress > 1) throw new Error('progress must be between 0 and 1');
    await await this.videoDecoder.seek(progress * this.duration() * 1000);
    this.currentFrame = Math.floor(progress * this.videoSequence.frameCount);
    const frameDataOpts = await this.getFrameData();
    this.flush(frameDataOpts);
  }
  /**
   * 渲染当前进度画面
   */
  public flush(frameDataOpts: FrameDataOptions) {
    this.flushInternal(frameDataOpts);
    this.eventManager.emit(EventName.onAnimationUpdate);
    return true;
  }

  protected flushInternal(frameDataOpts: FrameDataOptions) {}

  private flushLoop() {
    const frameDataOpts = this.videoDecoder.getFrameData();
    if (frameDataOpts === null) {
      this.renderTimer = setTimeout(() => {
        this.flushLoop();
      }, 0);
      return;
    }
    this.flush(frameDataOpts);
    this.currentFrame += 1;
    this.renderTimer = setTimeout(() => {
      this.flushLoop();
    }, this.flushBaseTime);
  }

  private clearRenderTimer() {
    if (this.renderTimer) {
      clearTimeout(this.renderTimer);
      this.renderTimer = null;
    }
  }

  private repeat() {
    // 循环结束
    if (this.repeatCount === 0) {
      this.clearRenderTimer();
      this.clearRender();
      this.videoDecoder.seek(0);
      this.playing = false;
      this.eventManager.emit('onAnimationEnd');
      return;
    }
    // 次数循环
    this.repeatCount -= 1;
    this.videoDecoder.seek(0);
    this.eventManager.emit('onAnimationRepeat');
    return;
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
