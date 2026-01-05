/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import { JPAGSurface } from 'libpag.so'
import { image } from '@kit.ImageKit';

export class PAGSurface {
  private constructor(nativeSurface: JPAGSurface) {
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

  static FromSurfaceID(surfaceId: number): PAGSurface | null {
    let nativeSurface = JPAGSurface.FromSurfaceID(surfaceId);
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
   * Update the size of the surface. This method should be called when the size of the surface is changed.
   */
  updateSize(): void {
    this.nativeSurface.updateSize();
  }

  /**
   * Free up resources used by the PAGSurface instance immediately instead of relying on the
   * garbage collector to do this for you at some point in the future.
   */
  release(): void {
    this.nativeSurface.release();
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
