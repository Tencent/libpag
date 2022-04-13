import { DEFAULT_CANVAS_SIZE } from '../constant';
import { PAG } from '../types';

export class GlobalCanvas {
  public static module: PAG;

  private _canvas: HTMLCanvasElement | OffscreenCanvas | null = null;
  private _contextId = 0;
  private width = DEFAULT_CANVAS_SIZE;
  private height = DEFAULT_CANVAS_SIZE;
  private retainCount = 0;

  public retain() {
    if (this.retainCount === 0) {
      try {
        this._canvas = new OffscreenCanvas(0, 0);
      } catch (error) {
        this._canvas = document.createElement('canvas');
      }
      this._canvas.width = this.width;
      this._canvas.height = this.height;
      this._contextId = GlobalCanvas.module.GL.createContext(this._canvas, { majorVersion: 1, minorVersion: 0 });
    }
    this.retainCount += 1;
  }

  public release() {
    this.retainCount -= 1;
    if (this.retainCount === 0) {
      if (this._contextId === 0) return;
      GlobalCanvas.module.GL.deleteContext(this._contextId);
      this._contextId = 0;
      this._canvas = null;
    }
  }

  public get canvas() {
    return this._canvas;
  }

  public get contextID() {
    return this._contextId;
  }

  public setCanvasSize(width = DEFAULT_CANVAS_SIZE, height = DEFAULT_CANVAS_SIZE) {
    this.width = width;
    this.height = height;
    if (this._contextId > 0 && this._canvas) {
      this._canvas.width = width;
      this._canvas.height = height;
    }
  }
}
