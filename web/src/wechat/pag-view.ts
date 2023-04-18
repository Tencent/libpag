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
}

export type wxCanvas = (HTMLCanvasElement | OffscreenCanvas) & {
  requestAnimationFrame: (callback: () => void) => number;
  cancelAnimationFrame: (requestID: number) => void;
};

@destroyVerify
@wasmAwaitRewind
export class PAGView extends NativePAGView {
  /**
   * Create pag view.
   * @param file pag file.
   * @param canvas target render canvas.
   * @param initOptions pag view options
   * @returns
   */
  public static async init(
    file: PAGComposition,
    canvas: HTMLCanvasElement | OffscreenCanvas,
    initOptions: PAGViewOptions = {},
  ): Promise<PAGView> {
    const pagPlayer = PAGModule.PAGPlayer.create();
    const pagView = new PAGView(pagPlayer, canvas);
    pagView.pagViewOptions = { ...pagView.pagViewOptions, ...initOptions };
    pagView.resetSize();
    pagView.renderCanvas = RenderCanvas.from(canvas, { alpha: true });
    pagView.renderCanvas.retain();
    pagView.pagGlContext = BackendContext.from(pagView.renderCanvas.glContext as BackendContext);
    pagView.frameRate = file.frameRate();
    pagView.pagSurface = this.makePAGSurface(pagView.pagGlContext, canvas.width, canvas.height);
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
  };

  /**
   * Update size when changed canvas size.
   */
  public updateSize() {
    if (!this.pagGlContext) return;
    this.resetSize();
    const pagSurface = PAGView.makePAGSurface(this.pagGlContext, this.canvasElement!.width, this.canvasElement!.height);
    this.player.setSurface(pagSurface);
    this.pagSurface?.destroy();
    this.pagSurface = pagSurface;
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

  protected override resetSize() {
    const dpr = wx.getSystemInfoSync().pixelRatio;
    this.canvasElement!.width = this.canvasElement!.width * dpr;
    this.canvasElement!.height = this.canvasElement!.height * dpr;
  }
}
