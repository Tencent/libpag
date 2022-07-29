import { getVideoParam, getWechatNetwork } from '../view/utils';
import { addListener, removeListener, removeAllListeners } from '../view/video-listener';
import { RenderingMode, EventName, ScaleMode } from '../types';
import { IS_IOS } from '../constant';
import { coverToMp4 } from '../generator/mp4-box-helper';
import { EventManager, Listener } from '../base/utils/event-manager';
import { destroyVerify } from '../decorators';
import { removeFile, touchDirectory, writeFile } from './file-utils';

import type { PAGFile } from '../pag-file';
import type { VideoParam } from '../types';
import type { VideoSequence } from '../base/video-sequence';
import type { FrameDataOptions, VideoDecoder, wx } from './types';

declare const wx: wx;

declare const setTimeout: (callback: () => void, delay: number) => number;

export interface RenderOptions {
  renderingMode?: RenderingMode;
}

const MP4_CACHE_PATH = `${wx.env.USER_DATA_PATH}/pag/`;

@destroyVerify
export class View {
  protected canvas: HTMLCanvasElement | null;
  protected videoParam: VideoParam;
  protected viewportSize = { x: 0, y: 0, width: 0, height: 0, scaleX: 1, scaleY: 1 }; // viewport尺寸 WebGL坐标轴轴心在左下角|Canvas2D坐标轴轴心在左上角
  protected canvasSize = { width: 0, height: 0 };

  private videoSequence: VideoSequence;
  private renderingMode: RenderingMode;
  private renderTimer: number | null = null;
  private playing = false;
  private destroyed = false;
  private repeatCount = 0; // 设置动画重复的次数。默认值为 0，只播放一次。如为 -1 动画则无限播放。
  private eventManager: EventManager;
  private viewScaleMode = ScaleMode.LetterBox;

  private mp4Path: string;
  private videoDecoder: VideoDecoder;
  private flushBaseTime;
  private currentFrame = 0;

  public constructor(pagFile: PAGFile, canvas: HTMLCanvasElement, options: RenderOptions) {
    const videoSequence = pagFile.getVideoSequence();
    if (!videoSequence) throw new Error('PAGFile has no BMP video sequence!');
    delete videoSequence.composition;
    this.videoSequence = videoSequence;
    console.log(this.videoSequence);

    this.canvas = canvas;
    this.videoParam = getVideoParam(pagFile, this.videoSequence);
    this.eventManager = new EventManager();
    this.renderingMode = options.renderingMode || RenderingMode.WebGL;
    this.setScaleMode();

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
   * 是否播放中
   */
  public isPlaying() {
    return this.playing;
  }
  /**
   * 是否已经销毁
   */
  public isDestroyed() {
    return this.destroyed;
  }
  /**
   * 动画持续时间
   */
  public duration() {
    return this.videoSequence.frameCount / this.videoSequence.frameRate;
  }
  /**
   * 动画持续时间
   */
  public frameRate() {
    return this.videoSequence.frameRate;
  }
  /**
   * 设置动画重复的次数。默认值为 1，只播放一次。如为 0 动画则无限播放。
   */
  public setRepeatCount(repeatCount = 1) {
    this.repeatCount = repeatCount < 0 ? -1 : repeatCount - 1;
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
    await this.videoDecoderSeek(progress * this.duration() * 1000);
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
  /**
   * 增加事件监听
   */
  public addListener(eventName: string, listener: Listener) {
    return this.eventManager.on(eventName, listener);
  }
  /**
   * 移除事件监听
   */
  public removeListener(eventName: string, listener?: Listener) {
    return this.eventManager.off(eventName, listener);
  }
  /**
   * 返回当前缩放模式
   */
  public scaleMode() {
    return this.viewScaleMode;
  }
  /**
   * 指定缩放内容的模式
   */
  public setScaleMode(scaleMode: ScaleMode = ScaleMode.LetterBox) {
    this.viewScaleMode = scaleMode;
    switch (scaleMode) {
      case ScaleMode.None:
        this.viewportSize = {
          x: 0,
          y: this.renderingMode === RenderingMode.WebGL ? this.canvas!.height - this.videoParam.height : 0,
          width: this.videoParam.width,
          height: this.videoParam.height,
          scaleX: 1,
          scaleY: 1,
        };
        break;
      case ScaleMode.Stretch:
        this.viewportSize = {
          x: 0,
          y: 0,
          width: this.canvas!.width,
          height: this.canvas!.height,
          scaleX: this.canvas!.width / this.videoParam.sequenceWidth,
          scaleY: this.canvas!.height / this.videoParam.sequenceHeight,
        };
        break;
      case ScaleMode.LetterBox:
        {
          const scaleX = this.canvas!.width / this.videoParam.sequenceWidth;
          const scaleY = this.canvas!.height / this.videoParam.sequenceHeight;
          const scale = Math.min(scaleX, scaleY);
          this.viewportSize = {
            x: (this.canvas!.width - this.videoParam.sequenceWidth * scale) / 2,
            y: (this.canvas!.height - this.videoParam.sequenceHeight * scale) / 2,
            width: this.videoParam.sequenceWidth * scale,
            height: this.videoParam.sequenceHeight * scale,
            scaleX: scale,
            scaleY: scale,
          };
        }
        break;
      case ScaleMode.Zoom:
        {
          const scaleX = this.canvas!.width / this.videoParam.sequenceWidth;
          const scaleY = this.canvas!.height / this.videoParam.sequenceHeight;
          const scale = Math.max(scaleX, scaleY);
          this.viewportSize = {
            x: (this.canvas!.width - this.videoParam.sequenceWidth * scale) / 2,
            y: (this.canvas!.height - this.videoParam.sequenceHeight * scale) / 2,
            width: this.videoParam.sequenceWidth * scale,
            height: this.videoParam.sequenceHeight * scale,
            scaleX: scale,
            scaleY: scale,
          };
        }
        break;
      default:
        break;
    }
  }

  public updateSize() {
    const styles = getComputedStyle(this.canvas as HTMLCanvasElement);
    this.canvas!.width = Number(styles.getPropertyValue('width').replace('px', ''));
    this.canvas!.height = Number(styles.getPropertyValue('height').replace('px', ''));
  }

  protected loadContext() {}

  protected flushInternal(frameDataOpts: FrameDataOptions) {}

  protected clearRender() {}

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

  private videoDecoderSeek(position: number) {
    return new Promise<boolean>((resolve) => {
      const onSeeked = () => {
        this.videoDecoder.off('seek', onSeeked);
        resolve(true);
      };
      this.videoDecoder.on('seek', onSeeked);
      this.videoDecoder.seek(position);
    });
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
