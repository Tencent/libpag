import { PAG, PAGScaleMode, PAGViewListenerEvent } from './types';
import { PAGPlayer } from './pag-player';
import { EventManager, Listener } from './utils/event-manager';
import { PAGSurface } from './pag-surface';
import { PAGFile } from './pag-file';
/* #if _WECHAT
import { isWechatMiniProgram } from './utils/ua';
import { getWechatElementById, requestAnimationFrame, cancelAnimationFrame } from './utils/wechat-babel';
//#else */
// #endif

declare const wx;
export class PAGView {
  public static module: PAG;
  /**
   * Create pag view.
   */
  public static async init(file: PAGFile, canvas: string | HTMLCanvasElement | OffscreenCanvas): Promise<PAGView> {
    /* #if _WECHAT
    let canvasElement: any;
    if (typeof canvas === 'string') {
      canvasElement = await getWechatElementById(canvas);
    } else {
      canvasElement = canvas;
    }
    const dpr = wx.getSystemInfoSync().pixelRatio
    canvasElement.width = canvasElement.width * dpr;
    canvasElement.height = canvasElement.height * dpr;
    const width = canvasElement.width;
    const height = canvasElement.height;
    const gl = canvasElement.getContext('webgl', { alpha: true });
    const contextID = this.module.GL.registerContext(gl, { majorVersion: 1, minorVersion: 0 });
    const pagPlayer = await this.module._PAGPlayer.create();
    const pagView = new PAGView(pagPlayer);
    this.module.GL.makeContextCurrent(contextID);
    pagView.pagSurface = await this.module._PAGSurface.FromFrameBuffer(0, width, height, true);
    //#else */
    let canvasElement: HTMLCanvasElement;
    if (typeof canvas === 'string') {
      canvasElement = document.getElementById(canvas.substr(1)) as HTMLCanvasElement;
    } else if (canvas instanceof HTMLCanvasElement) {
      canvasElement = canvas;
    }
    canvasElement.style.width = `${canvasElement.width}px`;
    canvasElement.style.height = `${canvasElement.height}px`;
    canvasElement.width = canvasElement.width * window.devicePixelRatio;
    canvasElement.height = canvasElement.height * window.devicePixelRatio;
    const pagPlayer = await this.module._PAGPlayer.create();
    const pagView = new PAGView(pagPlayer);
    const gl = canvasElement.getContext('webgl');
    const contextID = this.module.GL.registerContext(gl, { majorVersion: 1, minorVersion: 0 });
    this.module.GL.makeContextCurrent(contextID);
    pagView.pagSurface = await this.module._PAGSurface.FromFrameBuffer(0, canvasElement.width, canvasElement.height, true);
    // #endif
    pagView.player.setSurface(pagView.pagSurface);
    pagView.player.setComposition(file);
    await pagView.setProgress(0);
    pagView.eventManager = new EventManager();
    return pagView;
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
  private timer: number = null;
  private player: PAGPlayer;
  private pagSurface: PAGSurface;
  private repeatedTimes = 0;
  private eventManager: EventManager = null;

  public constructor(pagPlayer: PAGPlayer) {
    this.player = pagPlayer;
  }

  /**
   * The duration of current composition in microseconds.
   */
  public async duration() {
    return await this.player.duration();
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
    this.isPlaying = true;
    this.startTime = Date.now() * 1000 - this.playTime;
    await this.flushLoop();
  }
  /**
   * Pause the animation.
   */
  public pause() {
    if (!this.isPlaying || this.isDestroyed) return;
    this.clearTimer();
    this.isPlaying = false;
  }
  /**
   * Stop the animation.
   */
  public async stop(notification = true) {
    if (this.isDestroyed) return;
    this.clearTimer();
    this.playTime = 0;
    await this.player.setProgress(0);
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
  public setRepeatCount(repeatCount) {
    this.repeatCount = repeatCount < 0 ? 0 : repeatCount - 1;
  }
  /**
   * Returns the current progress of play position, the value is from 0.0 to 1.0. It is applied only
   * when the composition is not null.
   */
  public async getProgress() {
    return await this.player.getProgress();
  }
  /**
   * Set the progress of play position, the value is from 0.0 to 1.0.
   */
  public async setProgress(progress): Promise<number> {
    this.playTime = progress * (await this.duration());
    this.startTime = Date.now() * 1000 - this.playTime;
    if (!this.isPlaying) {
      await this.player.setProgressAndFlush(progress);
    }
    return progress;
  }
  /**
   * Return the value of videoEnabled property.
   */
  public async videoEnabled(): Promise<boolean> {
    return await this.player.videoEnabled();
  }
  /**
   * If set false, will skip video layer drawing.
   */
  public async setVideoEnabled(enable: boolean) {
    await this.player.setVideoEnabled(enable);
  }
  /**
   * If set to true, PAG renderer caches an internal bitmap representation of the static content for
   * each layer. This caching can increase performance for layers that contain complex vector content.
   * The execution speed can be significantly faster depending on the complexity of the content, but
   * it requires extra graphics memory. The default value is true.
   */
  public async cacheEnabled(): Promise<boolean> {
    return await this.player.cacheEnabled();
  }
  /**
   * Set the value of cacheEnabled property.
   */
  public async setCacheEnabled(enable: boolean) {
    await this.player.setCacheEnabled(enable);
  }
  /**
   * This value defines the scale factor for internal graphics caches, ranges from 0.0 to 1.0. The
   * scale factors less than 1.0 may result in blurred output, but it can reduce the usage of graphics
   * memory which leads to better performance. The default value is 1.0.
   */
  public async cacheScale(): Promise<number> {
    return await this.player.cacheScale();
  }
  /**
   * Set the value of cacheScale property.
   */
  public async setCacheScale(value: number) {
    await this.player.setCacheScale(value);
  }
  /**
   * The maximum frame rate for rendering. If set to a value less than the actual frame rate from
   * PAGFile, it drops frames but increases performance. Otherwise, it has no effect. The default
   * value is 60.
   */
  public async maxFrameRate(): Promise<number> {
    return await this.player.maxFrameRate();
  }
  /**
   * Set the maximum frame rate for rendering.
   */
  public async setMaxFrameRate(value: number) {
    await this.player.setMaxFrameRate(value);
  }
  /**
   * Returns the current scale mode.
   */
  public async scaleMode(): Promise<PAGScaleMode> {
    return await this.player.scaleMode();
  }
  /**
   * Specifies the rule of how to scale the pag content to fit the surface size. The matrix
   * changes when this method is called.
   */
  public async setScaleMode(value: PAGScaleMode) {
    await this.player.setScaleMode(value);
  }
  /**
   * Call this method to render current position immediately. If the play() method is already
   * called, there is no need to call it. Returns true if the content has changed.
   */
  public async flush() {
    await this.player.flush();
  }
  /**
   * Free the cache created by the pag view immediately. Can be called to reduce memory pressure.
   */
  public async freeCache() {
    await this.player.freeCache();
  }
  /**
   * Returns the current PAGComposition for PAGView to render as content.
   */
  public async getComposition() {
    return await this.player.getComposition();
  }
  /**
   * Update size when changed canvas size.
   */
  public async updateSize() {
    return this.pagSurface.updateSize();
  }

  public async destroy() {
    if (this.isDestroyed) return;
    this.clearTimer();
    await this.player.destroy();
    await this.pagSurface.destroy();
    this.isDestroyed = true;
  }

  private async flushLoop() {
    if (!this.isPlaying) {
      return;
    }
    /* #if _WECHAT
    this.timer = requestAnimationFrame(async () => {
      await this.flushLoop();
    });
    //#else */
    this.timer = window.requestAnimationFrame(async () => {
      await this.flushLoop();
    });
    // #endif
    await this.flushNextFrame();
  }

  private async flushNextFrame() {
    const duration = await this.duration();
    const count = Math.floor(this.playTime / duration);
    if (this.repeatCount >= 0 && count > this.repeatCount) {
      await this.stop(false);
      this.eventManager.emit(PAGViewListenerEvent.onAnimationEnd, this);
    } else {
      if (this.repeatedTimes < count) {
        this.eventManager.emit(PAGViewListenerEvent.onAnimationRepeat, this);
      }
      this.playTime = Date.now() * 1000 - this.startTime;
      await this.player.setProgressAndFlush((this.playTime % duration) / duration);
    }
    this.repeatedTimes = count;
  }

  private clearTimer(): void {
    if (this.timer) {
      /* #if _WECHAT
      cancelAnimationFrame(this.timer);
      //#else */
      window.cancelAnimationFrame(this.timer);
      // #endif
      this.timer = null;
    }
  }
}
