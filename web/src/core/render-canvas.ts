import { WEBGL_CONTEXT_ATTRIBUTES } from '../constant';
import { BackendContext } from './backend-context';

const renderCanvasList: RenderCanvas[] = [];

export class RenderCanvas {
  public static from(canvas: HTMLCanvasElement | OffscreenCanvas, contextAttributes?: WebGLContextAttributes) {
    let renderCanvas = renderCanvasList.find((targetCanvas) => targetCanvas.canvas === canvas);
    if (renderCanvas) return renderCanvas;
    renderCanvas = new RenderCanvas(canvas, contextAttributes);
    renderCanvasList.push(renderCanvas);
    return renderCanvas;
  }

  private _canvas: HTMLCanvasElement | OffscreenCanvas | null = null;
  private _glContext: BackendContext | null = null;
  private retainCount = 0;

  public constructor(canvas: HTMLCanvasElement | OffscreenCanvas, contextAttributes?: WebGLContextAttributes) {
    this._canvas = canvas;
    const gl = canvas.getContext('webgl2', {
      ...WEBGL_CONTEXT_ATTRIBUTES,
      ...contextAttributes,
    }) as WebGL2RenderingContext;
    if (!gl) throw new Error('Canvas context is not WebGL2!');
    this._glContext = BackendContext.from(gl);
  }

  public retain() {
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
}
