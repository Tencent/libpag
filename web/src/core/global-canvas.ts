import { DEFAULT_CANVAS_SIZE, WEBGL_CONTEXT_ATTRIBUTES } from '../constant';
import { BackendContext } from './backend-context';

export class GlobalCanvas {
  private _canvas: HTMLCanvasElement | OffscreenCanvas | null = null;
  private _glContext: BackendContext | null = null;
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

      const gl = this._canvas.getContext('webgl2', WEBGL_CONTEXT_ATTRIBUTES) as WebGL2RenderingContext;
      if (!gl) throw new Error('Canvas context is not WebGL2!');
      this._glContext = BackendContext.from(gl);
    }
    this.retainCount += 1;
  }

  public release() {
    this.retainCount -= 1;
    if (this.retainCount === 0) {
      if (!this._glContext) return;
      this._glContext.destroy();
      this._glContext = null;
      this._canvas = null;
    }
  }

  public get canvas() {
    return this._canvas;
  }

  public get glContext() {
    return this._glContext;
  }

  public setCanvasSize(width = DEFAULT_CANVAS_SIZE, height = DEFAULT_CANVAS_SIZE) {
    this.width = width;
    this.height = height;
    if (this._glContext && this._canvas) {
      this._canvas.width = width;
      this._canvas.height = height;
    }
  }
}
