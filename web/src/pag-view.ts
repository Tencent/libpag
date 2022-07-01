import { PAGScaleMode, PAGViewListenerEvent } from './types';
import { PAGPlayer } from './pag-player';
import { EventManager, Listener } from './utils/event-manager';
import { PAGSurface } from './pag-surface';
import { destroyVerify } from './utils/decorators';
import { Log } from './utils/log';
import { ErrorCode } from './utils/error-map';
import { isOffscreenCanvas } from './utils/type-utils';
import { BackendContext } from './core/backend-context';
import { PAGModule } from './binding';

import type { PAGComposition } from './pag-composition';
import type { Matrix } from './core/matrix';
import { WEBGL_CONTEXT_ATTRIBUTES } from './constant';

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
    } else if (canvas instanceof HTMLCanvasElement) {
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
      if (!PAGModule.globalCanvas.glContext) throw new Error('GlobalCanvas context is not WebGL!');
      pagView.pagGlContext = BackendContext.from(PAGModule.globalCanvas.glContext);
    } else {
      const gl = canvasElement.getContext('webgl', WEBGL_CONTEXT_ATTRIBUTES);
      if (!gl) throw new Error('Canvas context is not WebGL!');
      pagView.pagGlContext = BackendContext.from(gl);
    }
    pagView.resetSize(pagView.pagViewOptions.useScale);
    pagView.frameRate = file.frameRate();
    pagView.pagSurface = this.makePAGSurface(pagView.pagGlContext, pagView.rawWidth, pagView.rawHeight);
    pagView.player.setSurface(pagView.pagSurface);
    pagView.player.setComposition(file);
    pagView.setProgress(0);
    if (pagView.pagViewOptions.firstFrame) await pagView.flush();
    return pagView;
  }

  private static makePAGSurface(pagGlContext: BackendContext, width: number, height: number): PAGSurface {
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

  private startTime = 0;
  private playTime = 0;
  private timer: number | null = null;
  private player: PAGPlayer;
  private pagSurface: PAGSurface | null = null;
  private repeatedTimes = 0;
  private eventManager: EventManager = new EventManager();
  private pagGlContext: BackendContext | null = null;
  private canvasElement: HTMLCanvasElement | OffscreenCanvas | null;
  private canvasContext: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D | null | undefined;
  private rawWidth = 0;
  private rawHeight = 0;
  private currentFrame = 0;
  private frameRate = 0;
  private pagViewOptions: PAGViewOptions = {
    useScale: true,
    useCanvas2D: false,
    firstFrame: true,
  };

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
    if (this.isPlaying) return;
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
    if (!this.isPlaying) return;
    this.clearTimer();
    this.isPlaying = false;
    this.eventManager.emit(PAGViewListenerEvent.onAnimationPause, this);
  }
  /**
   * Stop the animation.
   */
  public async stop(notification = true) {
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
  public setProgress(progress: number): number {
    this.playTime = progress * this.duration();
    this.startTime = performance.now() * 1000 - this.playTime;
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
    const res = await this.player.flushInternal((res) => {
      if (this.pagViewOptions.useCanvas2D && res && PAGModule.globalCanvas.canvas) {
        if (!this.canvasContext) this.canvasContext = this.canvasElement?.getContext('2d');
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
    });
    if (res) {
      this.eventManager.emit(PAGViewListenerEvent.onAnimationFlushed, this);
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
      Log.errorByCode(ErrorCode.CanvasElementIsNoFound);
      return;
    }
    this.rawWidth = this.canvasElement.width;
    this.rawHeight = this.canvasElement.height;
    if (!this.pagGlContext) return null;
    const pagSurface = PAGView.makePAGSurface(this.pagGlContext, this.rawWidth, this.rawHeight);
    this.player.setSurface(pagSurface);
    this.pagSurface?.destroy();
    this.pagSurface = pagSurface;
  }

  public destroy() {
    this.clearTimer();
    this.player.destroy();
    this.pagSurface?.destroy();
    if (this.pagViewOptions.useCanvas2D) {
      PAGModule.globalCanvas.release();
    }
    this.pagGlContext?.destroy();
    this.pagGlContext = null;
    this.canvasContext = null;
    this.canvasElement = null;
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
    const currentFrame = Math.floor((this.playTime / 1000000) * this.frameRate);
    const count = Math.floor(this.playTime / duration);
    if (this.repeatCount >= 0 && count > this.repeatCount) {
      this.clearTimer();
      this.playTime = 0;
      this.isPlaying = false;
      this.eventManager.emit(PAGViewListenerEvent.onAnimationEnd, this);
      this.repeatedTimes = 0;
      return;
    }
    if (this.repeatedTimes === count && this.currentFrame === currentFrame) {
      return;
    }
    if (this.repeatedTimes < count) {
      this.eventManager.emit(PAGViewListenerEvent.onAnimationRepeat, this);
    }
    this.player.setProgress((this.playTime % duration) / duration);
    await this.flush();
    this.currentFrame = currentFrame;
    this.repeatedTimes = count;
    this.eventManager.emit(PAGViewListenerEvent.onAnimationUpdate, this);
  }

  private clearTimer(): void {
    if (this.timer) {
      window.cancelAnimationFrame(this.timer);
      this.timer = null;
    }
  }

  private resetSize(useScale = true) {
    if (!this.canvasElement) {
      Log.errorByCode(ErrorCode.CanvasElementIsNoFound);
      return;
    }

    if (!useScale || isOffscreenCanvas(this.canvasElement)) {
      this.rawWidth = this.canvasElement.width;
      this.rawHeight = this.canvasElement.height;
      return;
    }

    let displaySize: { width: number; height: number };
    const canvas = this.canvasElement as HTMLCanvasElement;
    const styleDeclaration = window.getComputedStyle(canvas, null);
    const computedSize = {
      width: Number(styleDeclaration.width.replace('px', '')),
      height: Number(styleDeclaration.height.replace('px', '')),
    };
    if (computedSize.width > 0 && computedSize.height > 0) {
      displaySize = computedSize;
    } else {
      const styleSize = {
        width: Number(canvas.style.width.replace('px', '')),
        height: Number(canvas.style.height.replace('px', '')),
      };
      if (styleSize.width > 0 && styleSize.height > 0) {
        displaySize = styleSize;
      } else {
        displaySize = {
          width: canvas.width,
          height: canvas.height,
        };
      }
    }

    canvas.style.width = `${displaySize.width}px`;
    canvas.style.height = `${displaySize.height}px`;
    this.rawWidth = displaySize.width * window.devicePixelRatio;
    this.rawHeight = displaySize.height * window.devicePixelRatio;
    canvas.width = this.rawWidth;
    canvas.height = this.rawHeight;
  }
}
