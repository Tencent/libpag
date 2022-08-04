import { Context, RenderOptions } from './context';
import { getWechatNetwork } from './utils';
import { EventName } from '../types';
import { IS_IOS } from '../constant';
import { destroyVerify } from '../decorators';

import type { PAGFile } from '../pag-file';
import { VideoReader } from './video-reader';
import { VideoSequence } from '../base/video-sequence';

declare global {
  interface Window {
    WeixinJSBridge?: any;
  }
}

const IS_WECHAT = navigator && /MicroMessenger/i.test(navigator.userAgent);

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
export class View extends Context {
  protected videoReader: VideoReader;

  public constructor(pagFile: PAGFile, canvas: HTMLCanvasElement, options: RenderOptions) {
    super(pagFile, canvas, options);
    this.videoReader = this.createVideoReader(this.videoSequence);
  }

  /**
   * 开始播放
   */
  public async play() {
    if (this.playing) return;
    this.playing = true;
    await this.videoReader.start();
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
    this.videoReader.pause();
    this.clearRenderTimer();
    this.playing = false;
    this.eventManager.emit(EventName.onAnimationPause);
  }
  /**
   * 停止播放
   */
  public stop() {
    this.videoReader.pause();
    this.videoReader.seek(0);
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
    this.videoReader.destroy();
    this.destroyed = true;
  }
  /**
   * 返回当前播放进度位置，取值范围为 0.0 到 1.0。
   */
  public getProgress() {
    return this.videoReader.progress();
  }
  /**
   * 设置播放进度位置，取值范围为 0.0 到 1.0。
   */
  public setProgress(progress: number) {
    if (progress < 0 || progress > 1) throw new Error('progress must be between 0.0 and 1.0!');
    return this.videoReader.seek(progress * this.videoReader.duration());
  }
  /**
   * 渲染当前进度画面
   */
  public flush() {
    this.flushInternal();
    this.eventManager.emit(EventName.onAnimationUpdate);
    return true;
  }

  protected flushInternal() {}

  protected createVideoReader(videoSequence: VideoSequence) {
    const videoReader = new VideoReader(videoSequence);
    if (!IS_IOS) {
      videoReader.addListener('ended', () => {
        this.repeat();
      });
    }
    return videoReader;
  }

  protected async repeat() {
    // 循环结束
    if (this.repeatCount === 0) {
      this.videoReader.pause();
      this.clearRenderTimer();
      this.clearRender();
      this.playing = false;
      this.eventManager.emit('onAnimationEnd');
      return;
    }
    // 次数循环
    this.repeatCount -= 1;
    if (IS_IOS) {
      await this.videoReader.seek(0);
    } else {
      this.videoReader.start();
    }
    this.eventManager.emit('onAnimationRepeat');
    return;
  }

  protected flushLoop() {
    this.renderTimer = window.requestAnimationFrame(() => {
      this.flushLoop();
    });
    if (IS_IOS && this.duration() - this.videoReader.currentTime() <= 1 / this.frameRate()) {
      this.repeat();
    }
    this.flush();
  }

  protected clearRenderTimer() {
    if (this.renderTimer) {
      window.cancelAnimationFrame(this.renderTimer);
      this.renderTimer = null;
    }
  }
}
