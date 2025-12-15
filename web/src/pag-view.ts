import { DebugData, PAGScaleMode, PAGViewEventMap, PAGViewListenerEvent } from './types';
import { PAGPlayer } from './pag-player';
import { EventManager, Listener } from './utils/event-manager';
import { PAGSurface } from './pag-surface';
import { destroyVerify } from './utils/decorators';
import { BackendContext } from './core/backend-context';
import { PAGModule } from './pag-module';
import { RenderCanvas } from './core/render-canvas';
import { Clock } from './utils/clock';
import { isInstanceOf } from './utils/type-utils';

import type { PAGComposition } from './pag-composition';
import type { Matrix } from './core/matrix';
import { calculateDisplaySize, isOffscreenCanvas } from '@tgfx/utils/canvas';

export interface PAGViewOptions {
  /**
   * Use style to scale canvas. default false.
   * When target canvas is offscreen canvas, useScale is false.
   */
  useScale?: boolean;
  /**
   * Can choose Canvas2D mode in chrome. default false.
   */
  useCanvas2D?: boolean;
  /**
   * Render first frame when pag view init. default true.
   */
  firstFrame?: boolean;
}

@destroyVerify
export class PAGView {
  /**
   * Create pag view.
   * @param file pag file.
   * @param canvas target render canvas.
   * @param initOptions pag view options
   * @returns
   */
  public static async init(
    file: PAGComposition,
    canvas: string | HTMLCanvasElement | OffscreenCanvas,
    initOptions: PAGViewOptions = {},
  ): Promise<PAGView | undefined> {
    let canvasElement: HTMLCanvasElement | OffscreenCanvas | null = null;
    if (typeof canvas === 'string') {
      canvasElement = document.getElementById(canvas.substr(1)) as HTMLCanvasElement;
    } else if (typeof window !== 'undefined' && isInstanceOf(canvas, globalThis.HTMLCanvasElement)) {
      canvasElement = canvas;
    } else if (isOffscreenCanvas(canvas)) {
      canvasElement = canvas;
    }
    if (!canvasElement) throw new Error('Canvas is not found!');

    const pagPlayer = PAGModule.PAGPlayer.create();
    const pagView = new PAGView(pagPlayer, canvasElement);
    pagView.pagViewOptions = { ...pagView.pagViewOptions, ...initOptions };

    if (pagView.pagViewOptions.useCanvas2D) {
      PAGModule.globalCanvas.retain();
      pagView.pagGlContext = BackendContext.from(PAGModule.globalCanvas.glContext as BackendContext);
    } else {
      pagView.renderCanvas = RenderCanvas.from(canvasElement);
      pagView.renderCanvas.retain();
      pagView.pagGlContext = BackendContext.from(pagView.renderCanvas.glContext as BackendContext);
    }
    pagView.resetSize(pagView.pagViewOptions.useScale);
    pagView.frameRate = file.frameRate();
    pagView.pagSurface = this.makePAGSurface(pagView.pagGlContext, pagView.rawWidth, pagView.rawHeight);
    pagView.player.setSurface(pagView.pagSurface);
    pagView.player.setComposition(file);
    pagView.setProgress(0);
    if (pagView.pagViewOptions.firstFrame) {
      await pagView.flush();
      pagView.playFrame = 0;
    }
    return pagView;
  }

  protected static makePAGSurface(pagGlContext: BackendContext, width: number, height: number): PAGSurface {
    if (!pagGlContext.makeCurrent()) throw new Error('Make context current fail!');
    const pagSurface = PAGSurface.fromRenderTarget(0, width, height, true);
    pagGlContext.clearCurrent();
    return pagSurface;
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

  protected pagViewOptions: PAGViewOptions = {
    useScale: true,
    useCanvas2D: false,
    firstFrame: true,
  };
  protected renderCanvas: RenderCanvas | null = null;
  protected pagGlContext: BackendContext | null = null;
  protected frameRate = 0;
  protected pagSurface: PAGSurface | null = null;
  protected player: PAGPlayer;
  protected playFrame = -1;
  protected canvasElement: HTMLCanvasElement | OffscreenCanvas | null;
  protected timer: number | null = null;
  protected flushingNextFrame = false;
  protected playTime = 0;
  protected startTime = 0;
  protected repeatedTimes = 0;
  protected eventManager: EventManager<PAGViewEventMap, PAGView> = new EventManager();

  private canvasContext: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D | null | undefined;
  private rawWidth = 0;
  private rawHeight = 0;
  private debugData: DebugData = {
    FPS: 0,
    flushTime: 0,
  };
  private fpsBuffer: number[] = [];

  public constructor(pagPlayer: PAGPlayer, canvasElement: HTMLCanvasElement | OffscreenCanvas) {
    this.player = pagPlayer;
    this.canvasElement = canvasElement;
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
  public addListener<K extends keyof PAGViewEventMap>(eventName: K, listener: Listener<PAGViewEventMap[K]>) {
    return this.eventManager.on(eventName, listener);
  }
  /**
   * Removes a listener from the set listening to this animation.
   */
  public removeListener<K extends keyof PAGViewEventMap>(eventName: K, listener?: Listener<PAGViewEventMap[K]>) {
    return this.eventManager.off(eventName, listener);
  }
  /**
   * Start the animation.
   */
  public async play() {
    if (this.isPlaying) return;
    this.isPlaying = true;
    this.startTime = this.getNowTime() * 1000 - this.playTime;
    for (const videoReader of this.player.videoReaders) {
      videoReader.isPlaying = true;
    }
    const playTime = this.playTime;
    await this.flushLoop(true);
    if (playTime === 0) {
      this.eventManager.emit('onAnimationStart', this);
    }
    this.eventManager.emit('onAnimationPlay', this);
    if (this.playFrame === 0) {
      this.eventManager.emit('onAnimationUpdate', this);
    }
  }
  /**
   * Pause the animation.
   */
  public pause() {
    if (!this.isPlaying) return;
    this.clearTimer();
    for (const videoReader of this.player.videoReaders) {
      videoReader.pause();
    }
    this.isPlaying = false;
    this.eventManager.emit('onAnimationPause', this);
  }
  /**
   * Stop the animation.
   */
  public async stop(notification = true) {
    this.clearTimer();
    this.playTime = 0;
    this.player.setProgress(0);
    this.playFrame = 0;
    await this.flush();
    for (const videoReader of this.player.videoReaders) {
      videoReader.stop();
    }
    this.isPlaying = false;
    if (notification) {
      this.eventManager.emit('onAnimationCancel', this);
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
   * Returns the current frame.
   */
  public currentFrame(): number {
    return this.player.currentFrame();
  }
  /**
   * Set the progress of play position, the value is from 0.0 to 1.0.
   */
  public setProgress(progress: number): number {
    this.playTime = progress * this.duration();
    this.startTime = this.getNowTime() * 1000 - this.playTime;
    if (!this.isPlaying) {
      this.player.setProgress(progress);
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
    const clock = new Clock();
    const res = await this.player.flushInternal((res) => {
      if (this.pagViewOptions.useCanvas2D && res && PAGModule.globalCanvas.canvas) {
        if (!this.canvasContext) this.canvasContext = this.canvasElement?.getContext('2d') as CanvasRenderingContext2D;
        const compositeOperation = this.canvasContext!.globalCompositeOperation;
        this.canvasContext!.globalCompositeOperation = 'copy';
        this.canvasContext?.drawImage(
          PAGModule.globalCanvas.canvas,
          0,
          PAGModule.globalCanvas.canvas.height - this.rawHeight,
          this.rawWidth,
          this.rawHeight,
          0,
          0,
          this.canvasContext.canvas.width,
          this.canvasContext.canvas.height,
        );
        this.canvasContext!.globalCompositeOperation = compositeOperation;
      }
      clock.mark('flush');
      this.setDebugData({ flushTime: clock.measure('', 'flush') });
      this.updateFPS();
    });
    this.eventManager.emit('onAnimationUpdate', this);
    if (res) {
      this.eventManager.emit('onAnimationFlushed', this);
    }
    return res;
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
  public getComposition() {
    return this.player.getComposition();
  }
  /**
   * Sets a new PAGComposition for PAGView to render as content.
   * Note: If the composition is already added to another PAGView, it will be removed from
   * the previous PAGView.
   */
  public setComposition(pagComposition: PAGComposition) {
    this.player.setComposition(pagComposition);
  }
  /**
   * Returns a copy of current matrix.
   */
  public matrix() {
    return this.player.matrix();
  }
  /**
   * Set the transformation which will be applied to the composition. The scaleMode property
   * will be set to PAGScaleMode::None when this method is called.
   */
  public setMatrix(matrix: Matrix) {
    this.player.setMatrix(matrix);
  }

  public getLayersUnderPoint(localX: number, localY: number) {
    return this.player.getLayersUnderPoint(localX, localY);
  }
  /**
   * Update size when changed canvas size.
   */
  public updateSize() {
    if (!this.canvasElement) {
      throw new Error('Canvas element is not found!');
    }
    this.rawWidth = this.canvasElement.width;
    this.rawHeight = this.canvasElement.height;
    if (!this.pagGlContext) return;
    const pagSurface = PAGView.makePAGSurface(this.pagGlContext, this.rawWidth, this.rawHeight);
    this.player.setSurface(pagSurface);
    this.pagSurface?.destroy();
    this.pagSurface = pagSurface;
  }
  /**
   * Prepares the player for the next flush() call. It collects all CPU tasks from the current
   * progress of the composition and runs them asynchronously in parallel. It is usually used for
   * speeding up the first frame rendering.
   */
  public prepare() {
    return this.player.prepare();
  }
  /**
   * Returns a ImageBitmap object capturing the contents of the PAGView. Subsequent rendering of
   * the PAGView will not be captured. Returns null if the PAGView hasn't been presented yet.
   */
  public async makeSnapshot() {
    return await createImageBitmap(this.canvasElement!);
  }

  public destroy() {
    this.clearTimer();
    this.player.destroy();
    this.pagSurface?.destroy();
    if (this.pagViewOptions.useCanvas2D) {
      PAGModule.globalCanvas.release();
    } else {
      this.renderCanvas?.release();
    }
    this.pagGlContext?.destroy();
    this.pagGlContext = null;
    this.canvasContext = null;
    this.canvasElement = null;
    this.isDestroyed = true;
  }

  public getDebugData() {
    return this.debugData;
  }

  public setDebugData(data: DebugData) {
    this.debugData = { ...this.debugData, ...data };
  }

  protected async flushLoop(force = false) {
    if (!this.isPlaying) {
      return;
    }
    this.setTimer();
    if (this.flushingNextFrame) return;
    try {
      this.flushingNextFrame = true;
      await this.flushNextFrame(force);
      this.flushingNextFrame = false;
    } catch (e: any) {
      this.flushingNextFrame = false;
      if (e.message !== 'The play() request was interrupted because the document was hidden!') {
        this.clearTimer();
      }
      console.error(e);
    }
  }

  protected async flushNextFrame(force = false) {
    const duration = this.duration();
    this.playTime = this.getNowTime() * 1000 - this.startTime;
    const playFrame = Math.floor((this.playTime / 1000000) * this.frameRate);
    const count = Math.floor(this.playTime / duration);
    if (!force && this.repeatCount >= 0 && count > this.repeatCount) {
      this.clearTimer();
      this.player.setProgress(1);
      await this.flush();
      this.playTime = 0;
      this.isPlaying = false;
      this.repeatedTimes = 0;
      this.eventManager.emit('onAnimationEnd', this);
      return true;
    }
    if (!force && this.repeatedTimes === count && this.playFrame === playFrame) {
      return false;
    }
    if (this.repeatedTimes < count) {
      this.eventManager.emit('onAnimationRepeat', this);
    }
    this.player.setProgress((this.playTime % duration) / duration);
    const res = await this.flush();
    if (this.needResetStartTime()) {
      // Decoding BMP takes too much time and makes the video reader seek repeatedly.
      this.startTime = this.getNowTime() * 1000 - this.playTime;
    }
    this.playFrame = playFrame;
    this.repeatedTimes = count;
    return res;
  }

  protected getNowTime() {
    try {
      return performance.now();
    } catch {
      return Date.now();
    }
  }

  protected setTimer() {
      this.timer = globalThis.requestAnimationFrame(() => {
        this.flushLoop();
      });
  }

  protected clearTimer(): void {
    if (this.timer) {
      globalThis.cancelAnimationFrame(this.timer);
      this.timer = null;
    }
  }

  protected resetSize(useScale = true) {
    if (!this.canvasElement) {
      throw new Error('Canvas element is not found!');
    }
    if (!useScale || isOffscreenCanvas(this.canvasElement)) {
      this.rawWidth = this.canvasElement.width;
      this.rawHeight = this.canvasElement.height;
      return;
    }
    const canvas = this.canvasElement as HTMLCanvasElement;
    const displaySize = calculateDisplaySize(canvas);
    canvas.style.width = `${displaySize.width}px`;
    canvas.style.height = `${displaySize.height}px`;
    this.rawWidth = displaySize.width * globalThis.devicePixelRatio;
    this.rawHeight = displaySize.height * globalThis.devicePixelRatio;
    canvas.width = this.rawWidth;
    canvas.height = this.rawHeight;
  }

  protected needResetStartTime() {
    for (const VideoReader of this.player.videoReaders) {
      if (VideoReader.isSought) return true;
    }
    return false;
  }

  private updateFPS() {
    let now: number;
    try {
      now = performance.now();
    } catch {
      now = Date.now();
    }
    this.fpsBuffer = this.fpsBuffer.filter((value) => now - value <= 1000);
    this.fpsBuffer.push(now);
    this.setDebugData({ FPS: this.fpsBuffer.length });
  }


}
