import { isAndroid } from './constant';
import { FrameData, VideoReader } from './video-reader';
import { EventName, RenderingMode } from '../types';
import { PAGWebGLView } from '../view/pag-webgl-view';
import { PAGFile } from '../pag-file';
import { VideoSequence } from '../base/video-sequence';
import { Clock } from '../base/utils/clock';

declare const setTimeout: (callback: () => void, delay: number) => number;
declare const setInterval: (callback: () => void, delay: number) => number;

const ANDROID_16_ALIGN = 16;

export class PAGView extends PAGWebGLView {
  public static init(data: ArrayBuffer, canvas: HTMLCanvasElement) {
    const clock = new Clock();
    const pagFile = PAGFile.fromArrayBuffer(data);
    clock.mark('decode');
    const pagView = new PAGView(pagFile, canvas);
    pagView.setDebugData({ decodePAGFile: clock.measure('', 'decode') });
    return pagView;
  }

  private flushBaseTime;
  private frameData: FrameData | undefined = undefined;
  private needGetFrame = false;
  private mark = 0;

  public constructor(pagFile: PAGFile, canvas: HTMLCanvasElement) {
    super(pagFile, canvas, { renderingMode: RenderingMode.WebGL });
    this.flushBaseTime = Math.floor(1000 / this.videoSequence.frameRate);
    this.videoReader.start();
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
    this.clearTimer();
    this.playing = false;
    this.eventManager.emit(EventName.onAnimationPause);
  }
  /**
   * 停止播放
   */
  public stop() {
    this.clearTimer();
    this.seekToStart();
    this.clearRender();
    this.playing = false;
    this.eventManager.emit(EventName.onAnimationCancel);
  }
  /**
   * 销毁播放实例
   */
  public destroy() {
    this.clearTimer();
    this.videoReader.destroy();
    this.clearRender();
    this.canvas = null;
    this.destroyed = true;
  }

  public updateSize() {
    // NOP
  }
  /**
   * 设置播放进度位置，取值范围为 0.0 到 1.0。
   */
  public setProgress(progress: number) {
    if (progress < 0 || progress > 1) throw new Error('progress must be between 0.0 and 1.0!');
    const currentFrame = Math.round(progress * this.videoSequence.frameCount);
    if (this.currentFrame !== currentFrame) {
      if (currentFrame !== this.currentFrame + 1) {
        this.needSeek = true;
      }
      this.needGetFrame = true;
      this.currentFrame = currentFrame;
      return;
    }
    this.needSeek = false;
    this.needGetFrame = false;
  }

  protected async flushInternal() {
    const draw = () => {
      const drawMark = Date.now();
      this.draw();
      this.setDebugData({ draw: Date.now() - drawMark });
      this.updateFPS();
      this.eventManager.emit(EventName.onAnimationUpdate);
    };

    if (this.needSeek) {
      this.needSeek = false;
      await this.videoReader.seek((this.currentFrame / this.frameRate()) * 1000);
    }
    if (this.needGetFrame) {
      this.needGetFrame = false;
      const getFrameMark = Date.now();
      this.videoReader.getFrameData((frameData: FrameData) => {
        this.frameData = frameData;
        this.currentFrame = frameData.id;

        this.setDebugData({ getFrame: Date.now() - getFrameMark });
        if (isAndroid) {
          this.scale = {
            x: (Math.ceil(this.frameData.width / ANDROID_16_ALIGN) * ANDROID_16_ALIGN) / this.frameData.width,
            y: (Math.ceil(this.frameData.height / ANDROID_16_ALIGN) * ANDROID_16_ALIGN) / this.frameData.height,
          };
        }
        draw();
        if (this.currentFrame === this.videoSequence.frameCount - 1) {
          this.repeat();
        }
        console.log(`flush dst: ${Date.now() - this.mark}ms`);
        this.mark = Date.now();
      });
      return;
    }
    draw();
    console.log(`flush dst: ${Date.now() - this.mark}ms`);
    this.mark = Date.now();
  }

  protected override flushLoop() {
    const loop = async () => {
      this.needGetFrame = true;
      this.flushInternal();
    };
    this.renderTimer = setInterval(loop, this.flushBaseTime);
    return Promise.resolve();
  }

  protected override detectWebGLContext() {
    return true;
  }

  protected override createVideoReader(videoSequence: VideoSequence) {
    const { videoReader, debugData } = VideoReader.create(videoSequence);
    this.setDebugData(debugData);
    return videoReader;
  }

  protected override texImage2D() {
    this.gl.texImage2D(
      this.gl.TEXTURE_2D,
      0,
      this.gl.RGBA,
      this.frameData!.width,
      this.frameData!.height,
      0,
      this.gl.RGBA,
      this.gl.UNSIGNED_BYTE,
      new Uint8Array(this.frameData!.data),
    );
  }

  protected override repeat() {
    // end
    if (this.repeatCount === 0) {
      this.clearTimer();
      this.seekToStart();
      this.playing = false;
      this.eventManager.emit('onAnimationEnd');
      return Promise.resolve(false);
    }
    this.repeatCount -= 1;
    this.seekToStart();
    this.eventManager.emit('onAnimationRepeat');
    return Promise.resolve(true);
  }

  protected override clearTimer() {
    if (this.renderTimer) {
      clearTimeout(this.renderTimer);
      this.renderTimer = null;
    }
    this.videoReader.clearCallback();
  }

  protected override updateFPS() {
    const now = Date.now();
    this.fpsBuffer = this.fpsBuffer.filter((value) => now - value <= 1000);
    this.fpsBuffer.push(now);
    this.setDebugData({ FPS: this.fpsBuffer.length });
  }

  private async seekToStart() {
    this.videoReader.seek(0);
    this.currentFrame = -1;
  }
}
