import { PAGModule } from '../pag-module';
import { PAGView as NativePAGView } from '../pag-view';

import { RenderCanvas } from '../core/render-canvas';
import { BackendContext } from '../core/backend-context';
import { destroyVerify, wasmAwaitRewind } from '../utils/decorators';

import type { PAGComposition } from '../pag-composition';
import type { wx } from './interfaces';

declare const wx: wx;

export interface PAGViewOptions {
  /**
   * Render first frame when pag view init. default true.
   */
  firstFrame?: boolean;
  /**
   * Use style to scale canvas. default false.
   * When target canvas is offscreen canvas, useScale is false.
   */
  useScale?: boolean;
}

export type wxCanvas = (HTMLCanvasElement | OffscreenCanvas) & {
  requestAnimationFrame: (callback: () => void) => number;
  cancelAnimationFrame: (requestID: number) => void;
};

function isOffscreenCanvas(canvas: any): boolean {
  if (canvas.isOffscreenCanvas !== undefined) {
    return canvas.isOffscreenCanvas;
  }
  return false;
}

interface CanvasSize {
  width: number;
  height: number;
}

@destroyVerify
@wasmAwaitRewind
export class PAGView extends NativePAGView {
  /**
   * Get canvas element by canvas ID
   *
   * @param {string} canvasId - The ID of the canvas element to find
   *
   * @returns {Promise<HTMLCanvasElement | OffscreenCanvas | null>}
   *          - Returns the found canvas element, or null if not found
   */
  public static getCanvasElementByCanvasId(canvasId: string): Promise<HTMLCanvasElement | OffscreenCanvas> {
    return new Promise((resolve, reject) => {
      const query = wx.createSelectorQuery();
      query
        .select(`#${canvasId}`)
        .fields({ node: true, size: true })
        .exec((res) => {
          if (res[0] && res[0].node) {
            resolve(res[0].node);
          } else {
            reject(new Error(`Cannot find Canvas element with ID ${canvasId}`));
          }
        });
    });
  }

  /**
   * Create pag view.
   * @param file pag file.
   * @param canvas target render canvas.
   * @param initOptions pag view options
   * @returns
   */
  public static async init(
    file: PAGComposition,
    canvas: HTMLCanvasElement | OffscreenCanvas | string,
    initOptions: PAGViewOptions = {},
  ): Promise<PAGView> {
    let canvasElement: HTMLCanvasElement | OffscreenCanvas | null = null;
    if (typeof canvas === 'string') {
      canvasElement = await this.getCanvasElementByCanvasId(canvas);
    } else {
      canvasElement = canvas;
    }
    if (!canvasElement) throw new Error('Canvas is not found!');

    const pagPlayer = PAGModule.PAGPlayer.create();
    const pagView = new PAGView(pagPlayer, canvasElement);
    if (typeof canvas === 'string') {
      pagView.canvasId = canvas;
    }
    pagView.pagViewOptions = { ...pagView.pagViewOptions, ...initOptions };
    await pagView.resetSize(pagView.pagViewOptions.useScale);
    pagView.renderCanvas = RenderCanvas.from(canvasElement, { alpha: true });
    pagView.renderCanvas.retain();
    pagView.pagGlContext = BackendContext.from(pagView.renderCanvas.glContext as BackendContext);
    pagView.frameRate = file.frameRate();
    pagView.pagSurface = this.makePAGSurface(pagView.pagGlContext, canvasElement.width, canvasElement.height);
    pagView.player.setSurface(pagView.pagSurface);
    pagView.player.setComposition(file);
    pagView.setProgress(0);
    if (pagView.pagViewOptions.firstFrame) {
      await pagView.flush();
      pagView.playFrame = 0;
    }
    return pagView;
  }

  protected override pagViewOptions: PAGViewOptions = {
    firstFrame: true,
    useScale: true,
  };

  private canvasId = '';

  /**
   * Update size when changed canvas size.
   */
  public updateSize() {
    if (!this.canvasElement) {
      throw new Error('Canvas element is not found!');
    }
    if (!this.pagGlContext) return;
    const pagSurface = PAGView.makePAGSurface(this.pagGlContext, this.canvasElement!.width, this.canvasElement!.height);
    this.player.setSurface(pagSurface);
    this.pagSurface?.destroy();
    this.pagSurface = pagSurface;
  }

  public getCanvasCssSize(canvasId: string): Promise<CanvasSize> {
    return new Promise((resolve) => {
      wx.createSelectorQuery()
        .select(`#${canvasId}`)
        .fields({ node: true, size: true })
        .exec((res) => {
          resolve({
            width: res[0]?.width || 0,
            height: res[0]?.height || 0,
          });
        });
    });
  }

  public async calculateDisplaySize(canvas: any) {
    if (this.canvasId !== undefined && this.canvasId !== '') {
      const canvasSize = await this.getCanvasCssSize(this.canvasId);
      if (canvasSize.width !== 0 && canvasSize.height !== 0) {
        return canvasSize;
      }
    }
    if (canvas.displayWidth === undefined && canvas.displayHeight === undefined) {
      canvas.displayWidth = canvas.width;
      canvas.displayHeight = canvas.height;
    }
    return {
      width: canvas.displayWidth,
      height: canvas.displayHeight,
    };
  }

  protected override async flushLoop(force = false) {
    if (!this.isPlaying) return;
    this.setTimer();
    if (this.flushingNextFrame) return;
    try {
      this.flushingNextFrame = true;
      await this.flushNextFrame(force);
      this.flushingNextFrame = false;
    } catch (e: any) {
      this.flushingNextFrame = false;
      throw e;
    }
  }

  protected override async flushNextFrame(force = false) {
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
    this.playFrame = playFrame;
    this.repeatedTimes = count;
    return res;
  }

  protected override getNowTime() {
    return Date.now();
  }

  protected override setTimer() {
    this.timer = (this.canvasElement as wxCanvas).requestAnimationFrame(() => {
      this.flushLoop();
    });
  }

  protected override clearTimer() {
    if (this.timer) {
      (this.canvasElement as wxCanvas).cancelAnimationFrame(this.timer);
      this.timer = null;
    }
  }

  protected override async resetSize(useScale = true) {
    if (!this.canvasElement) {
      throw new Error('Canvas element is not found!');
    }
    if (!useScale || isOffscreenCanvas(this.canvasElement)) {
      return;
    }
    const displaySize = await this.calculateDisplaySize(this.canvasElement);
    const dpr = wx.getSystemInfoSync().pixelRatio;
    this.canvasElement!.width = displaySize.width * dpr;
    this.canvasElement!.height = displaySize.height * dpr;
  }
}
