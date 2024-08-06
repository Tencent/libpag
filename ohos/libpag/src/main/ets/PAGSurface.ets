import { JPAGSurface } from 'libpag.so'
import { PAGInit } from './PAGInit';
import { image } from '@kit.ImageKit';

export class PAGSurface {
  private constructor(nativeSurface: JPAGSurface) {
    PAGInit.Init();
    this.nativeSurface = nativeSurface;
  }

  static MakeOffscreen(width: number, height: number): PAGSurface | null {
    let nativeSurface = JPAGSurface.MakeOffscreen(width, height);
    if (nativeSurface == null) {
      return null;
    } else {
      return new PAGSurface(nativeSurface);
    }
  }

  /**
   * The width of surface in pixels.
   */
  width(): number {
    return this.nativeSurface.width();
  }

  /**
   * The height of surface in pixels.
   */
  height(): number {
    return this.nativeSurface.height();
  }

  /**
   * Erases all pixels of this surface with transparent color. Returns true if the content has changed.
   */
  clearAll(): boolean {
    return this.nativeSurface.clearAll();
  }

  /**
   * Free the cache created by the surface immediately. Call this method can reduce memory pressure.
   */
  freeCache(): void {
    return this.nativeSurface.freeCache();
  }

  /**
   * Returns a bitmap capturing the contents of the PAGSurface. Subsequent rendering of the
   * PAGSurface will not be captured.
   */
  public makeSnapshot(): image.PixelMap | null {
    return this.nativeSurface.makeSnapshot();
  }

  readonly nativeSurface: JPAGSurface;
}