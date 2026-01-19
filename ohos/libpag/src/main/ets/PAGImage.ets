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

import { resourceManager } from '@kit.LocalizationKit';
import { PAGUtils } from './private/PAGUtils';
import { Matrix4 } from '@kit.ArkUI';
import { PAGScaleMode } from './PAGScaleMode';
import { JPAGImage } from 'libpag.so'

export class PAGImage {
  private constructor(nativeImage: JPAGImage) {
    this.nativeImage = nativeImage;
  }

  /**
   * Creates a PAGImage object from a path of a image file, return null if the file does not exist or it's not a valid image file.
   * @param path
   * @returns
   */
  static FromPath(path: string): PAGImage | null {
    let nativeImage = JPAGImage.FromPath(path);
    if (nativeImage == null) {
      return null;
    } else {
      return new PAGImage(nativeImage);
    }
  }

  /**
   * Creates a PAGImage object from the specified byte data, return null if the bytes is empty or it's not a valid image file.
   * @param data
   * @returns
   */
  static FromBytes(data: ArrayBuffer): PAGImage | null {
    let nativeImage = JPAGImage.FromBytes(data);
    if (nativeImage == null) {
      return null;
    } else {
      return new PAGImage(nativeImage);
    }
  }

  /**
   * Creates a PAGImage object from pixelMap, return null if the pixelMap is empty or it's not valid.
   * @param pixelMap
   * @returns
   */
  static FromPixelMap(pixelMap: PixelMap): PAGImage | null {
    let nativeImage = JPAGImage.FromPixelMap(pixelMap);
    if (nativeImage == null) {
      return null;
    } else {
      return new PAGImage(nativeImage);
    }
  }

  /**
   * Load a pag file from assets, returns null if the file does not exist or the data is not a valid image file.
   * @param manager
   * @param name
   * @returns
   */
  static LoadFromAssets(manager: resourceManager.ResourceManager, name: string): PAGImage | null {
    let nativeImage = JPAGImage.LoadFromAssets(manager, name);
    if (nativeImage == null) {
      return null;
    } else {
      return new PAGImage(nativeImage);
    }
  }

  /**
   * Returns the width in pixels of the image.
   */
  width(): number {
    return this.nativeImage.width();
  }

  /**
   * Returns the height in pixels of the image.
   */
  height(): number {
    return this.nativeImage.height();
  }

  /**
   * Returns the current scale mode.
   */
  scaleMode(): PAGScaleMode {
    return this.nativeImage.scaleMode();
  }

  /**
   * Specifies the rule of how to scale the image content to fit the original image size.
   */
  setScaleMode(mode: PAGScaleMode) {
    this.nativeImage.setScaleMode(mode);
  }

  /**
   * Returns a copy of current matrix.
   */
  matrix(): Matrix4 {
    return PAGUtils.ToTsMatrix(this.nativeImage.matrix());
  }

  /**
   * Set the transformation which will be applied to the image content.
   */
  public setMatrix(matrix: Matrix4) {
    this.nativeImage.setMatrix(PAGUtils.ToNativeMatrix(matrix));
  }

  readonly nativeImage: JPAGImage;
}
