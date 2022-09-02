import { PAGModule } from '../pag-module';
import { PAGView as NativePAGView } from '../pag-view';

import { RenderCanvas } from '../core/render-canvas';
import { BackendContext } from '../core/backend-context';

import type { PAGComposition } from '../pag-composition';
import type { wx } from './interfaces';
import { destroyVerify, wasmAwaitRewind } from '../utils/decorators';

declare const wx: wx;
declare const setInterval: (callback: () => void, ms: number) => number;

export interface PAGViewOptions {
  /**
   * Render first frame when pag view init. default true.
   */
  firstFrame?: boolean;
}

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
      pagView.currentFrame = 0;
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
    const pagSurface = PAGView.makePAGSurface(this.pagGlContext, this.canvasElement!.width, this.canvasElement!.height);
    this.player.setSurface(pagSurface);
    this.pagSurface?.destroy();
    this.pagSurface = pagSurface;
  }

  protected override async flushLoop() {
    this.timer = setInterval(() => {
      this.flushNextFrame();
    }, 1000 / this.frameRate);
  }

  protected override getNowTime() {
    try {
      return wx.getPerformance().now();
    } catch (e) {
      return Date.now();
    }
  }

  protected override clearTimer() {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
  }
}
