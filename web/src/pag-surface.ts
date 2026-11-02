import { PAGModule } from './pag-module';
import { AlphaType, ColorType } from './types';
import { isWebGPUBackend } from './core/backend-context';
import {
  clearWebGPUTextureHandle,
  getWebGPUTextureHandle,
  unregisterWebGPUTexture,
  untrackWebGPUTextureHandle,
  WebGPUTextureLike,
  wrapBackendTexture,
} from './core/webgpu-texture';
import { GPU_TEXTURE_USAGE_COPY_SRC } from './constant';
import { destroyVerify } from './utils/decorators';

// Timeout for polling an async readback session. Generous enough for a stalled GPU queue while
// still surfacing a stuck readback instead of waiting forever.
const READBACK_TIMEOUT_MS = 30000;

// Throttles the unsupported-surface warning of readPixelsAsync() to a single console entry.
let readbackUnsupportedWarned = false;

// Throttles the not-supported warning of readPixels() on the WebGPU backend; the backend is a
// build-time constant, so without throttling a per-frame caller would spam the console.
let readPixelsUnsupportedWarned = false;

// Values must stay in sync with the declaration order of the pag::ReadbackStatus enum in
// src/rendering/PAGSurfaceReadback.h.
const READBACK_STATUS_READY = 0;
const READBACK_STATUS_PENDING = 1;
const READBACK_STATUS_FAILED = 2;

// Yields one turn of the event loop between readback polls. MessageChannel is not affected by
// the 4ms clamping browsers apply to nested setTimeout(0) chains; setTimeout remains the
// fallback for environments without MessageChannel.
const nextTick = (): Promise<void> => {
  if (typeof MessageChannel === 'undefined') {
    return new Promise((resolve) => setTimeout(resolve, 0));
  }
  return new Promise((resolve) => {
    const channel = new MessageChannel();
    channel.port1.onmessage = () => {
      channel.port1.close();
      channel.port2.close();
      resolve();
    };
    channel.port2.postMessage(null);
  });
};

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
      return new PAGSurface(wasmIns).markCanvasBacked();
    }
    const wasmIns = PAGModule._PAGSurface._FromCanvas(canvas);
    if (!wasmIns) throw new Error(`Make PAGSurface from canvas ${canvas} fail!`);
    return new PAGSurface(wasmIns).markCanvasBacked();
  }
  /**
   * Make a PAGSurface from texture. On the WebGL backend, pass the numeric texture handle
   * registered via BackendContext.registerTexture(). On the WebGPU backend, pass the host
   * GPUTexture directly; it must be created from the same GPUDevice that drives PAG rendering
   * (the device passed to PAGInit) and with both the RENDER_ATTACHMENT and COPY_SRC usage flags
   * (COPY_SRC is required for reading pixels back via readPixelsAsync()).
   */
  public static fromTexture(
    texture: number | WebGPUTextureLike,
    width: number,
    height: number,
    flipY: boolean,
  ): PAGSurface {
    if (typeof texture !== 'number' && ((texture.usage ?? 0) & GPU_TEXTURE_USAGE_COPY_SRC) === 0) {
      // A host texture is wrapped as-is without extra usage flags. Without COPY_SRC the
      // copyTextureToBuffer-based readback fails as a WebGPU validation error, leaving the
      // readback buffer untouched and making readPixelsAsync() silently return all-zero pixels.
      throw new Error(
        'PAGSurface.fromTexture() requires the GPUTexture to be created with the COPY_SRC usage flag.',
      );
    }
    return wrapBackendTexture(
      'PAGSurface.fromTexture()',
      texture,
      (textureID, format) => PAGModule._PAGSurface._FromTexture(textureID, width, height, flipY, format),
      (wasmIns) => new PAGSurface(wasmIns),
    );
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

  private isCanvasBacked = false;
  private readbackPending = false;

  public constructor(wasmIns: any) {
    this.wasmIns = wasmIns;
  }

  private markCanvasBacked(): PAGSurface {
    this.isCanvasBacked = true;
    return this;
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
      if (!readPixelsUnsupportedWarned) {
        readPixelsUnsupportedWarned = true;
        console.warn('PAGSurface.readPixels() is not supported on the WebGPU backend. Use readPixelsAsync() instead.');
      }
      return null;
    }
    if (colorType === ColorType.Unknown) return null;
    const rowBytes = this.wasmIns._width() * (colorType === ColorType.ALPHA_8 ? 1 : 4);
    return this.wasmIns._readPixels(colorType, alphaType, rowBytes);
  }
  /**
   * Asynchronously reads pixels from current PAGSurface with specified color type and alpha type.
   * On WebGL the returned promise resolves within the current frame; on WebGPU it resolves once
   * the GPU readback mapping completes. Rejects if the readback fails on the GPU side; returns
   * null when the surface is destroyed while waiting, when a readback is already pending, or
   * when the readback does not complete within timeoutMs (default 30000 ms, the polling deadline
   * for the GPU readback mapping; the session is discarded on timeout without side effects).
   *
   * Canvas-backed surfaces (created by fromCanvas(), including the surfaces used by PAGView on
   * the WebGPU backend) cannot be read back after the frame is presented, so this method always
   * returns null for them; call PAGView.makeSnapshot() instead, or render to an offscreen
   * PAGSurface and read from it. Only one readback may run per surface at a time; a concurrent
   * call returns null immediately.
   */
  public async readPixelsAsync(
    colorType: ColorType,
    alphaType: AlphaType,
    timeoutMs: number = READBACK_TIMEOUT_MS,
  ): Promise<Uint8Array | null> {
    if (colorType === ColorType.Unknown) return null;
    if (this.readbackPending) {
      console.warn(
        'PAGSurface.readPixelsAsync() was called while a previous readback on the same surface is still pending. Wait for it to settle before calling again.',
      );
      return null;
    }
    const session = PAGModule._AsyncReadPixels(this.wasmIns);
    if (!session) {
      if (this.isCanvasBacked && !readbackUnsupportedWarned) {
        readbackUnsupportedWarned = true;
        console.warn(
          'PAGSurface.readPixelsAsync() is not supported on canvas-backed surfaces. Use PAGView.makeSnapshot() or read from an offscreen PAGSurface instead.',
        );
      }
      return null;
    }
    this.readbackPending = true;
    try {
      const deadline = Date.now() + timeoutMs;
      let status = session._status();
      while (status === READBACK_STATUS_PENDING) {
        if (this.isDestroyed || Date.now() > deadline) return null;
        await nextTick();
        status = session._status();
      }
      if (status === READBACK_STATUS_FAILED) {
        throw new Error('PAGSurface.readPixelsAsync() failed because the GPU readback did not complete successfully.');
      }
      const rowBytes = session._width() * (colorType === ColorType.ALPHA_8 ? 1 : 4);
      return session._readPixels(colorType, alphaType, rowBytes) as Uint8Array | null;
    } finally {
      this.readbackPending = false;
      session.delete();
    }
  }

  public destroy(): void {
    this.wasmIns.delete();
    const webGPUTextureHandle = getWebGPUTextureHandle(this.wasmIns);
    if (webGPUTextureHandle > 0) {
      clearWebGPUTextureHandle(this.wasmIns);
      unregisterWebGPUTexture(webGPUTextureHandle);
      untrackWebGPUTextureHandle(this);
    }
    this.isDestroyed = true;
  }
}
