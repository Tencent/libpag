import { AlphaType, ColorType, PAG } from './types';
import { destroyVerify, wasmAwaitRewind } from './utils/decorators';

@destroyVerify
@wasmAwaitRewind
export class PAGSurface {
  public static module: PAG;

  public static FromCanvas(canvasID: string): PAGSurface {
    const wasmIns = PAGSurface.module._PAGSurface._FromCanvas(canvasID);
    return new PAGSurface(wasmIns);
  }

  public static FromTexture(textureID: number, width: number, height: number, flipY: boolean): PAGSurface {
    const wasmIns = PAGSurface.module._PAGSurface._FromTexture(textureID, width, height, flipY);
    return new PAGSurface(wasmIns);
  }

  public static FromRenderTarget(frameBufferID: number, width: number, height: number, flipY: boolean): PAGSurface {
    const wasmIns = PAGSurface.module._PAGSurface._FromRenderTarget(frameBufferID, width, height, flipY);
    return new PAGSurface(wasmIns);
  }

  public wasmIns;
  public isDestroyed = false;

  public constructor(wasmIns: any) {
    this.wasmIns = wasmIns;
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
   * row bytes. Returns true if pixels are copied to dstPixels.
   */
  public readPixels(colorType: ColorType, alphaType: AlphaType): Uint8Array | null {
    if (colorType === ColorType.Unknown) return null;
    const rowBytes = this.width() * (colorType === ColorType.ALPHA_8 ? 1 : 4);
    const length = rowBytes * this.height();
    const dataUint8Array = new Uint8Array(length);
    const dataPtr = PAGSurface.module._malloc(dataUint8Array.byteLength);
    const dataOnHeap = new Uint8Array(PAGSurface.module.HEAPU8.buffer, dataPtr, dataUint8Array.byteLength);
    const res = this.wasmIns._readPixels(colorType, alphaType, dataPtr, rowBytes) as boolean;
    dataUint8Array.set(dataOnHeap);
    PAGSurface.module._free(dataPtr);
    return res ? dataUint8Array : null;
  }

  public destroy(): void {
    this.wasmIns.delete();
    this.isDestroyed = true;
  }
}
