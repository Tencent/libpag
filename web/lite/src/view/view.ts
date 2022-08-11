import { getVideoParam, getWechatNetwork } from './utils';
import { addListener, removeListener, removeAllListeners } from './video-listener';
import { RenderingMode, EventName, ScaleMode } from '../types';
import { IS_IOS } from '../constant';
import { coverToMp4 } from '../generator/mp4-box-helper';
import { PAGFile } from '../pag-file';
import { EventManager, Listener } from '../base/utils/event-manager';

import type { VideoParam } from '../types';
import type { VideoSequence } from '../base/video-sequence';
import { destroyVerify } from '../decorators';

declare global {
  interface Window {
    WeixinJSBridge?: any;
  }
}

export interface RenderOptions {
  renderingMode?: RenderingMode;
  useScale?: boolean;
}

const IS_WECHAT = /MicroMessenger/i.test(navigator.userAgent);

export const playVideoElement = async (videoElement: HTMLVideoElement) => {
  if (IS_WECHAT && window.WeixinJSBridge) {
    await getWechatNetwork();
  }
  try {
    await videoElement.play();
  } catch (error: any) {
    throw new Error(error.message);
  }
};

@destroyVerify
export class View {
  protected canvas: HTMLCanvasElement | null;
  protected videoParam: VideoParam;
  protected videoElement: HTMLVideoElement | null;
  protected viewportSize = { x: 0, y: 0, width: 0, height: 0, scaleX: 1, scaleY: 1 }; // viewport尺寸 WebGL坐标轴轴心在左下角|Canvas2D坐标轴轴心在左上角
  protected canvasSize = { width: 0, height: 0 };

  private videoSequence: VideoSequence;
  private videoCanPlay = false;
  private renderingMode: RenderingMode;
  private renderTimer: number | null = null;
  private playing = false;
  private destroyed = false;
  private repeatCount = 0; // 设置动画重复的次数。默认值为 0，只播放一次。如为 -1 动画则无限播放。
  private eventManager: EventManager;
  private viewScaleMode = ScaleMode.LetterBox;

  public constructor(pagFile: PAGFile, canvas: HTMLCanvasElement, options: RenderOptions) {
    const videoSequence = pagFile.getVideoSequence();
    if (!videoSequence) throw new Error('PAGFile has no BMP video sequence!');
    this.videoSequence = videoSequence;
    this.canvas = canvas;
    this.videoElement = document.createElement('video');
    this.videoElement.style.display = 'none';
    this.videoElement.muted = true;
    this.videoElement.playsInline = true;
    addListener(this.videoElement, 'canplay', () => {
      this.videoCanPlay = true;
    });
    if (!IS_IOS) {
      addListener(this.videoElement, 'ended', () => {
        this.repeat(false);
      });
    }
    this.videoElement.src = URL.createObjectURL(new Blob([coverToMp4(this.videoSequence)], { type: 'video/mp4' }));
    this.videoParam = getVideoParam(pagFile, this.videoSequence);
    this.eventManager = new EventManager();
    this.renderingMode = options.renderingMode || RenderingMode.WebGL;
    this.updateSize(options.useScale);
    this.setScaleMode();
  }

  /**
   * 开始播放
   */
  public async play() {
    if (this.playing) return;
    this.playing = true;
    await playVideoElement(this.videoElement as HTMLVideoElement);
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
    this.videoElement?.pause();
    this.clearRenderTimer();
    this.playing = false;
    this.eventManager.emit(EventName.onAnimationPause);
  }
  /**
   * 停止播放
   */
  public stop() {
    this.videoElement?.pause();
    this.videoElement!.currentTime = 0;
    this.clearRender();
    this.playing = false;
    this.eventManager.emit(EventName.onAnimationCancel);
  }
  /**
   * 销毁播放实例
   */
  public destroy() {
    this.clearRenderTimer();
    this.clearRender();
    this.canvas = null;
    if (this.videoElement) {
      removeAllListeners(this.videoElement);
      this.videoElement = null;
    }
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
    return Math.round((this.videoElement!.currentTime / this.videoElement!.duration) * 100) / 100;
  }
  /**
   * 设置播放进度位置，取值范围为 0.0 到 1.0。
   */
  public async setProgress(progress: number) {
    return new Promise<boolean>((resolve) => {
      if (progress < 0 || progress > 1) {
        console.error('progress must be between 0.0 and 1.0');
        resolve(false);
        return;
      }
      const seekCallback = () => {
        removeListener(this.videoElement as HTMLVideoElement, 'seeked', seekCallback);
        resolve(true);
      };
      addListener(this.videoElement as HTMLVideoElement, 'seeked', seekCallback);
      this.videoElement!.currentTime = progress * this.videoElement!.duration;
    });
  }
  /**
   * 渲染当前进度画面
   */
  public flush() {
    // Video 初始化未完成，跳过渲染
    if (!this.videoCanPlay) {
      console.warn('Video is not ready yet, skip rendering.');
      return false;
    }
    this.flushInternal();
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

  public updateSize(useScale = true) {
    if (!this.canvas) {
      throw new Error('Canvas element is not found!');
    }
    let displaySize: { width: number; height: number };
    const styleDeclaration = getComputedStyle(this.canvas as HTMLCanvasElement);
    const computedSize = {
      width: Number(styleDeclaration.width.replace('px', '')),
      height: Number(styleDeclaration.height.replace('px', '')),
    };
    if (computedSize.width > 0 && computedSize.height > 0) {
      displaySize = computedSize;
    } else {
      const styleSize = {
        width: Number(this.canvas.style.width.replace('px', '')),
        height: Number(this.canvas.style.height.replace('px', '')),
      };
      if (styleSize.width > 0 && styleSize.height > 0) {
        displaySize = styleSize;
      } else {
        displaySize = {
          width: this.canvas.width,
          height: this.canvas.height,
        };
      }
    }

    if (!useScale) {
      this.canvas!.width = this.canvas!.width || displaySize.width;
      this.canvas!.height = this.canvas!.height || displaySize.height;
      return;
    }
    this.canvas.style.width = `${displaySize.width}px`;
    this.canvas.style.height = `${displaySize.height}px`;
    this.canvas.width = displaySize.width * window.devicePixelRatio;
    this.canvas.height = displaySize.height * window.devicePixelRatio;
  }

  protected loadContext() {}

  protected flushInternal() {}

  protected clearRender() {}

  private flushLoop() {
    this.renderTimer = window.requestAnimationFrame(() => {
      this.flushLoop();
    });
    if (IS_IOS && this.duration() - this.videoElement!.currentTime <= 1 / this.frameRate()) {
      this.repeat(true);
    }
    this.flush();
  }

  private clearRenderTimer() {
    if (this.renderTimer) {
      window.cancelAnimationFrame(this.renderTimer);
      this.renderTimer = null;
    }
  }

  private repeat(isIOS = false) {
    // 循环结束
    if (this.repeatCount === 0) {
      this.videoElement?.pause();
      this.clearRenderTimer();
      this.clearRender();
      this.playing = false;
      this.eventManager.emit('onAnimationEnd');
      return;
    }
    // 次数循环
    this.repeatCount -= 1;
    if (isIOS) {
      this.videoElement!.currentTime = 0;
    } else {
      playVideoElement(this.videoElement as HTMLVideoElement);
    }
    this.eventManager.emit('onAnimationRepeat');
    return;
  }
}
