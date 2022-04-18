import { PAG } from './types';
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

  public static FromFrameBuffer(frameBufferID: number, width: number, height: number, flipY: boolean): PAGSurface {
    const wasmIns = PAGSurface.module._PAGSurface._FromFrameBuffer(frameBufferID, width, height, flipY);
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

  public destroy(): void {
    this.wasmIns.delete();
    this.isDestroyed = true;
  }
}
