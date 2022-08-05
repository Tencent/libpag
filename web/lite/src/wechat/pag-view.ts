import { VideoReader } from './video-reader';
import { DebugData, EventName, RenderingMode } from '../types';
import { PAGWebGLView } from '../view/pag-webgl-view';
import { PAGFile } from '../pag-file';
import { VideoSequence } from '../base/video-sequence';
import { FrameDataOptions } from './types';
import { Clock } from '../base/utils/clock';

declare const setTimeout: (callback: () => void, delay: number) => number;

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
  private currentFrame = 0;
  private frameDataOpts: FrameDataOptions | undefined = undefined;
  private getFrameMark = 0;
  private startTime = 0;

  public constructor(pagFile: PAGFile, canvas: HTMLCanvasElement) {
    super(pagFile, canvas, { renderingMode: RenderingMode.WebGL });
    this.flushBaseTime = Math.floor(1000 / this.videoSequence.frameRate);
    this.videoReader.addListener('ended', () => {
      this.repeat();
    });
  }
  /**
   * 开始播放
   */
  public async play() {
    if (this.playing) return;
    this.playing = true;
    this.startTime = Date.now() - this.currentFrame * this.flushBaseTime;
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
    this.seekToStart();
    this.clearRender();
    this.playing = false;
    this.eventManager.emit(EventName.onAnimationCancel);
  }
  /**
   * 销毁播放实例
   */
  public destroy() {
    this.clearRenderTimer();
    this.videoReader.destroy();
    this.clearRender();
    this.canvas = null;
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
    await await this.videoReader.seek(progress * this.duration() * 1000);
    this.currentFrame = Math.floor(progress * this.videoSequence.frameCount);
    this.frameDataOpts = await this.getFrameData();
    this.flush();
  }

  public updateSize() {
    // NOP
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
      this.frameDataOpts!.width,
      this.frameDataOpts!.height,
      0,
      this.gl.RGBA,
      this.gl.UNSIGNED_BYTE,
      new Uint8Array(this.frameDataOpts!.data),
    );
  }

  protected override async repeat() {
    // 循环结束
    if (this.repeatCount === 0) {
      this.clearRenderTimer();
      this.clearRender();
      await this.seekToStart();
      this.playing = false;
      this.eventManager.emit('onAnimationEnd');
      return;
    }
    // 次数循环
    this.repeatCount -= 1;
    await this.seekToStart();
    this.eventManager.emit('onAnimationRepeat');
    return;
  }

  protected override flushLoop() {
    if (this.getFrameMark === 0) this.getFrameMark = Date.now();
    const frameDataOpts = this.videoReader.getFrameData() as FrameDataOptions;
    if (frameDataOpts === null) {
      this.renderTimer = setTimeout(() => {
        this.flushLoop();
      }, 0);
      return;
    }
    this.frameDataOpts = frameDataOpts;
    this.setDebugData({ getFrame: Date.now() - this.getFrameMark });
    this.getFrameMark = 0;
    this.flush();
    this.currentFrame += 1;
    let nextRenderTime = Math.max(this.flushBaseTime * this.currentFrame + this.startTime - Date.now(), 1);
    this.renderTimer = setTimeout(() => {
      this.flushLoop();
    }, nextRenderTime);
  }

  protected override clearRenderTimer() {
    if (this.renderTimer) {
      clearTimeout(this.renderTimer);
      this.renderTimer = null;
    }
  }

  protected override updateFPS() {
    const now = Date.now();
    this.fpsBuffer = this.fpsBuffer.filter((value) => now - value <= 1000);
    this.fpsBuffer.push(now);
    this.setDebugData({ FPS: this.fpsBuffer.length });
  }

  private getFrameData() {
    return new Promise<FrameDataOptions>((resolve) => {
      const loop = () => {
        const frameData = this.videoReader.getFrameData();
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

  private async seekToStart() {
    await this.videoReader.seek(0);
    this.currentFrame = 0;
    this.startTime = 0;
  }
}
