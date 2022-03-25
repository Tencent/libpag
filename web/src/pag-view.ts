import { PAG, PAGScaleMode, PAGViewListenerEvent } from './types';
import { PAGPlayer } from './pag-player';
import { EventManager, Listener } from './utils/event-manager';
import { PAGSurface } from './pag-surface';
import { PAGFile } from './pag-file';
import { Log } from './utils/log';
import { ErrorCode } from './utils/error-map';
import { SCREEN_2560_RESOLUTION } from './constant';

export class PAGView {
  public static module: PAG;
  /**
   * Create pag view.
   */
  public static async init(
    file: PAGFile,
    canvas: string | HTMLCanvasElement | OffscreenCanvas,
  ): Promise<PAGView | undefined> {
    let canvasElement: HTMLCanvasElement | null = null;
    if (typeof canvas === 'string') {
      canvasElement = document.getElementById(canvas.substr(1)) as HTMLCanvasElement;
    } else if (canvas instanceof HTMLCanvasElement) {
      canvasElement = canvas;
    }
    if (!canvasElement) {
      Log.errorByCode(ErrorCode.CanvasIsNotFound);
    } else {
      const styleWidth = Number(canvasElement.style.width.replace('px', ''));
      const styleHeight = Number(canvasElement.style.height.replace('px', ''));
      const displayWidth = styleWidth > 0 ? styleWidth : canvasElement.width;
      const displayHeight = styleHeight > 0 ? styleHeight : canvasElement.width;
      const rawWidth = displayWidth * window.devicePixelRatio;
      const rawHeight = displayHeight * window.devicePixelRatio;

      if (rawWidth > SCREEN_2560_RESOLUTION || rawHeight > SCREEN_2560_RESOLUTION) {
        Log.warn(
          "Don't render the target larger than 2560 px resolution. It may be a render failure in the low graphic memory device.",
        );
      }
      canvasElement.style.width = `${displayWidth}px`;
      canvasElement.style.height = `${displayHeight}px`;
      canvasElement.width = rawWidth;
      canvasElement.height = rawHeight;

      const pagPlayer = this.module.PAGPlayer.create();
      const pagView = new PAGView(pagPlayer);
      const gl = canvasElement.getContext('webgl');
      if (!(gl instanceof WebGLRenderingContext)) {
        Log.errorByCode(ErrorCode.CanvasContextIsNotWebGL);
      } else {
        const contextID = this.module.GL.registerContext(gl, { majorVersion: 1, minorVersion: 0 });
        this.module.GL.makeContextCurrent(contextID);
        pagView.pagSurface = this.module.PAGSurface.FromFrameBuffer(0, canvasElement.width, canvasElement.height, true);
        pagView.player.setSurface(pagView.pagSurface);
        pagView.player.setComposition(file);
        await pagView.setProgress(0);
        return pagView;
      }
    }
  }

  /**
   * The repeat count of player.
   */
  public repeatCount = 0;
  /**
   * Indicates whether or not this pag view is playing.
   */
  public isPlaying = false;
  /**
   * Indicates whether or not this pag view is destroyed.
   */
  public isDestroyed = false;

  private startTime = 0;
  private playTime = 0;
  private timer: number | null = null;
  private player: PAGPlayer;
  private pagSurface: PAGSurface | undefined;
  private repeatedTimes = 0;
  private eventManager: EventManager = new EventManager();

  public constructor(pagPlayer: PAGPlayer) {
    this.player = pagPlayer;
  }

  /**
   * The duration of current composition in microseconds.
   */
  public duration() {
    return this.player.duration();
  }
  /**
   * Adds a listener to the set of listeners that are sent events through the life of an animation,
   * such as start, repeat, and end.
   */
  public addListener(eventName: PAGViewListenerEvent, listener: Listener) {
    return this.eventManager.on(eventName, listener);
  }
  /**
   * Removes a listener from the set listening to this animation.
   */
  public removeListener(eventName: PAGViewListenerEvent, listener?: Listener) {
    return this.eventManager.off(eventName, listener);
  }
  /**
   * Start the animation.
   */
  public async play() {
    if (this.isPlaying || this.isDestroyed) return;
    if (this.playTime === 0) {
      this.eventManager.emit(PAGViewListenerEvent.onAnimationStart, this);
    }
    this.eventManager.emit(PAGViewListenerEvent.onAnimationPlay, this);
    this.isPlaying = true;
    this.startTime = performance.now() * 1000 - this.playTime;
    await this.flushLoop();
  }
  /**
   * Pause the animation.
   */
  public pause() {
    if (!this.isPlaying || this.isDestroyed) return;
    this.clearTimer();
    this.isPlaying = false;
    this.eventManager.emit(PAGViewListenerEvent.onAnimationPause, this);
  }
  /**
   * Stop the animation.
   */
  public async stop(notification = true) {
    if (this.isDestroyed) return;
    this.clearTimer();
    this.playTime = 0;
    this.player.setProgress(0);
    await this.flush();
    this.isPlaying = false;
    if (notification) {
      this.eventManager.emit(PAGViewListenerEvent.onAnimationCancel, this);
    }
  }
  /**
   * Set the number of times the animation will repeat. The default is 1, which means the animation
   * will play only once. 0 means the animation will play infinity times.
   */
  public setRepeatCount(repeatCount: number) {
    this.repeatCount = repeatCount < 0 ? 0 : repeatCount - 1;
  }
  /**
   * Returns the current progress of play position, the value is from 0.0 to 1.0. It is applied only
   * when the composition is not null.
   */
  public getProgress(): number {
    return this.player.getProgress();
  }
  /**
   * Set the progress of play position, the value is from 0.0 to 1.0.
   */
  public async setProgress(progress: number): Promise<number> {
    this.playTime = progress * (await this.duration());
    this.startTime = performance.now() * 1000 - this.playTime;
    if (!this.isPlaying) {
      this.player.setProgress(progress);
      await this.flush();
    }
    return progress;
  }
  /**
   * Return the value of videoEnabled property.
   */
  public videoEnabled(): boolean {
    return this.player.videoEnabled();
  }
  /**
   * If set false, will skip video layer drawing.
   */
  public setVideoEnabled(enable: boolean) {
    this.player.setVideoEnabled(enable);
  }
  /**
   * If set to true, PAG renderer caches an internal bitmap representation of the static content for
   * each layer. This caching can increase performance for layers that contain complex vector content.
   * The execution speed can be significantly faster depending on the complexity of the content, but
   * it requires extra graphics memory. The default value is true.
   */
  public cacheEnabled(): boolean {
    return this.player.cacheEnabled();
  }
  /**
   * Set the value of cacheEnabled property.
   */
  public setCacheEnabled(enable: boolean) {
    this.player.setCacheEnabled(enable);
  }
  /**
   * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
   * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of graphics
   * memory which leads to better performance. The default value is 1.0.
   */
  public cacheScale(): number {
    return this.player.cacheScale();
  }
  /**
   * Set the value of cacheScale property.
   */
  public setCacheScale(value: number) {
    this.player.setCacheScale(value);
  }
  /**
   * The maximum frame rate for rendering. If set to a value less than the actual frame rate from
   * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
   * value is 60.
   */
  public maxFrameRate(): number {
    return this.player.maxFrameRate();
  }
  /**
   * Set the maximum frame rate for rendering.
   */
  public setMaxFrameRate(value: number) {
    this.player.setMaxFrameRate(value);
  }
  /**
   * Returns the current scale mode.
   */
  public scaleMode(): PAGScaleMode {
    return this.player.scaleMode();
  }
  /**
   * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
   * changes when this method is called.
   */
  public setScaleMode(value: PAGScaleMode) {
    this.player.setScaleMode(value);
  }
  /**
   * Call this method to render current position immediately. If the play() method is already
   * called, there is no need to call it. Returns true if the content has changed.
   */
  public async flush() {
    await this.player.flush();
    this.eventManager.emit(PAGViewListenerEvent.onAnimationFlushed, this);
  }
  /**
   * Free the cache created by the pag view immediately. Can be called to reduce memory pressure.
   */
  public freeCache() {
    this.pagSurface?.freeCache();
  }
  /**
   * Returns the current PAGComposition for PAGView to render as content.
   */
  public getComposition(): PAGFile {
    return this.player.getComposition();
  }
  /**
   * Update size when changed canvas size.
   */
  public updateSize() {
    this.pagSurface?.updateSize();
  }

  public destroy() {
    if (this.isDestroyed) return;
    this.clearTimer();
    this.player.destroy();
    this.pagSurface?.destroy();
    this.isDestroyed = true;
  }

  private async flushLoop() {
    if (!this.isPlaying) {
      return;
    }
    this.timer = window.requestAnimationFrame(async () => {
      await this.flushLoop();
    });
    await this.flushNextFrame();
  }

  private async flushNextFrame() {
    const duration = this.duration();
    this.playTime = performance.now() * 1000 - this.startTime;
    const count = Math.floor(this.playTime / duration);
    if (this.repeatCount >= 0 && count > this.repeatCount) {
      this.clearTimer();
      this.playTime = 0;
      this.isPlaying = false;
      this.eventManager.emit(PAGViewListenerEvent.onAnimationEnd, this);
    } else {
      if (this.repeatedTimes < count) {
        this.eventManager.emit(PAGViewListenerEvent.onAnimationRepeat, this);
      }
      this.player.setProgress((this.playTime % duration) / duration);
      await this.flush();
    }
    this.repeatedTimes = count;
  }

  private clearTimer(): void {
    if (this.timer) {
      window.cancelAnimationFrame(this.timer);
      this.timer = null;
    }
  }
}
