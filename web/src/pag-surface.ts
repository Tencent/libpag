import { PAGModule } from './pag-module';
import { AlphaType, ColorType } from './types';
import {
  isWebGPUBackend,
  resolveTextureHandle,
  unregisterWebGPUTexture,
  WebGPUTextureLike,
} from './core/backend-context';
import { destroyVerify } from './utils/decorators';

// Timeout for polling an async readback session. Generous enough for a stalled GPU queue while
// still surfacing a stuck readback instead of waiting forever.
const READBACK_TIMEOUT_MS = 30000;

@destroyVerify
export class PAGSurface {
  /**
   * Make a PAGSurface from canvas. Pass a canvas ID string on the WebGL backend. On the WebGPU
   * backend a canvas object (HTMLCanvasElement or OffscreenCanvas) can also be passed, which is
   * the only way to target a canvas that has no DOM id, such as an OffscreenCanvas.
   */
  public static fromCanvas(canvas: string | HTMLCanvasElement | OffscreenCanvas): PAGSurface {
    if (typeof canvas !== 'string') {
      if (!isWebGPUBackend()) {
        throw new Error(
          'PAGSurface.fromCanvas() with a canvas object is only supported on the WebGPU backend. Pass a canvas ID string on the WebGL backend.',
        );
      }
      const wasmIns = PAGModule._PAGSurface._FromCanvasObject!(canvas);
      if (!wasmIns) throw new Error('Make PAGSurface from canvas object fail!');
      return new PAGSurface(wasmIns);
    }
    const wasmIns = PAGModule._PAGSurface._FromCanvas(canvas);
    if (!wasmIns) throw new Error(`Make PAGSurface from canvas ${canvas} fail!`);
    return new PAGSurface(wasmIns);
  }
  /**
   * Make a PAGSurface from texture. On the WebGL backend, pass the numeric texture handle
   * registered via BackendContext.registerTexture(). On the WebGPU backend, pass the host
   * GPUTexture directly; it must be created from the same GPUDevice that drives PAG rendering
   * (the device passed to PAGInit) and with the RENDER_ATTACHMENT usage flag.
   */
  public static fromTexture(
    texture: number | WebGPUTextureLike,
    width: number,
    height: number,
    flipY: boolean,
  ): PAGSurface {
    const { textureID, format, webGPUHandle } = resolveTextureHandle('PAGSurface.fromTexture()', texture);
    const wasmIns = PAGModule._PAGSurface._FromTexture(textureID, width, height, flipY, format);
    if (!wasmIns) {
      if (webGPUHandle > 0) unregisterWebGPUTexture(webGPUHandle);
      throw new Error(`Make PAGSurface from texture fail!`);
    }
    return new PAGSurface(wasmIns, webGPUHandle);
  }
  /**
   * Make a PAGSurface from frameBuffer. WebGL backend only; on the WebGPU backend a texture view
   * is the render target, so use fromTexture() instead.
   */
  public static fromRenderTarget(frameBufferID: number, width: number, height: number, flipY: boolean): PAGSurface {
    if (isWebGPUBackend()) {
      throw new Error(
        'PAGSurface.fromRenderTarget() is not supported on the WebGPU backend. Use fromTexture() with a GPUTexture created with the RENDER_ATTACHMENT usage flag instead.',
      );
    }
    const wasmIns = PAGModule._PAGSurface._FromRenderTarget(frameBufferID, width, height, flipY);
    if (!wasmIns) throw new Error(`Make PAGSurface from frameBuffer ${frameBufferID} fail!`);
    return new PAGSurface(wasmIns);
  }

  public wasmIns;
  public isDestroyed = false;

  private webGPUTextureHandle = 0;

  public constructor(wasmIns: any, webGPUTextureHandle = 0) {
    this.wasmIns = wasmIns;
    this.webGPUTextureHandle = webGPUTextureHandle;
  }
  /**
   * The width of surface in pixels.
   */
  public width(): number {
    return this.wasmIns._width() as number;
  }
  /**
   * The height of surface in pixels.
   */
  public height(): number {
    return this.wasmIns._height() as number;
  }
  /**
   * Update the size of surface, and reset the internal surface.
   */
  public updateSize(): void {
    this.wasmIns._updateSize();
  }
  /**
   * Erases all pixels of this surface with transparent color. Returns true if the content has
   * changed.
   */
  public clearAll(): boolean {
    return this.wasmIns._clearAll() as boolean;
  }
  /**
   * Free the cache created by the surface immediately. Can be called to reduce memory pressure.
   */
  public freeCache(): void {
    this.wasmIns._freeCache();
  }
  /**
   * Copies pixels from current PAGSurface to dstPixels with specified color type, alpha type and
   * row bytes. Returns true if pixels are copied to dstPixels. Not supported on the WebGPU
   * backend (synchronous GPU readback is unavailable there); use readPixelsAsync() instead.
   */
  public readPixels(colorType: ColorType, alphaType: AlphaType): Uint8Array | null {
    if (isWebGPUBackend()) {
      console.warn('PAGSurface.readPixels() is not supported on the WebGPU backend. Use readPixelsAsync() instead.');
      return null;
    }
    if (colorType === ColorType.Unknown) return null;
    const rowBytes = this.wasmIns._width() * (colorType === ColorType.ALPHA_8 ? 1 : 4);
    return this.wasmIns._readPixels(colorType, alphaType, rowBytes);
  }
  /**
   * Asynchronously reads pixels from current PAGSurface with specified color type and alpha type.
   * Works on every backend: on WebGL the returned promise resolves within the current frame, on
   * WebGPU it resolves once the GPU readback mapping completes. Returns null on failure or when
   * the readback does not complete within 30 seconds. Do not call readPixelsAsync() again on the
   * same surface before the previous call settles; each concurrent session holds its own GPU
   * readback buffer.
   */
  public async readPixelsAsync(colorType: ColorType, alphaType: AlphaType): Promise<Uint8Array | null> {
    if (colorType === ColorType.Unknown) return null;
    const session = this.wasmIns._readPixelsAsync();
    if (!session) return null;
    try {
      const deadline = Date.now() + READBACK_TIMEOUT_MS;
      while (!session._isReady()) {
        if (Date.now() > deadline) return null;
        await new Promise((resolve) => setTimeout(resolve, 0));
      }
      const rowBytes = session._width() * (colorType === ColorType.ALPHA_8 ? 1 : 4);
      return session._readPixels(colorType, alphaType, rowBytes) as Uint8Array | null;
    } finally {
      session.delete();
    }
  }

  public destroy(): void {
    this.wasmIns.delete();
    if (this.webGPUTextureHandle > 0) {
      unregisterWebGPUTexture(this.webGPUTextureHandle);
      this.webGPUTextureHandle = 0;
    }
    this.isDestroyed = true;
  }
}
