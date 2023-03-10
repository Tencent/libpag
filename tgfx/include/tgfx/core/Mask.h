/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#pragma once

#include "tgfx/core/Font.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageBuffer.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/Stroke.h"
#include "tgfx/core/TextBlob.h"

namespace tgfx {
/**
 * Mask is a utility that can take an image described in a vector graphics format (paths, glyphs)
 * and convert it into a raster image that can be used as a drawing mask. Mask is not thread safe,
 * do not use it across multiple threads.
 */
class Mask {
 public:
  /**
   * Creates a new Mask with specified width and height. Returns nullptr if the size is less than
   * zero or too big.
   */
  static std::shared_ptr<Mask> Make(int width, int height, bool tryHardware = true);

  virtual ~Mask() = default;

  /**
   * Returns the width of the mask.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the height of the mask.
   */
  int height() const {
    return _height;
  }

  /**
   * Returns the current total matrix.
   */
  Matrix getMatrix() const {
    return matrix;
  }

  /**
   * Replaces transformation with specified matrix.
   */
  void setMatrix(const Matrix& m) {
    matrix = m;
  }

  /**
   * Adds the fills of the given path to this mask，with its top-left corner at (0, 0), using
   * current Matrix.
   */
  virtual void fillPath(const Path& path) = 0;

  /**
   * Adds the outlines of the given path to this mask，with its top-left corner at (0, 0), using
   * current Matrix.
   */
  virtual void strokePath(const Path& path, const Stroke& stroke);

  /**
   * Adds the fills of the given text blob to this mask，with its top-left corner at (0, 0), using
   * current Matrix. Returns false if the associated typeface has color glyphs and leaves the mask
   * unchanged.
   */
  virtual bool fillText(const TextBlob* textBlob);

  /**
   * Adds the outlines of the given text blob to this mask，with its top-left corner at (0, 0),
   * using the current Matrix. Returns false if the associated typeface has color glyphs and leaves
   * the mask unchanged.
   */
  virtual bool strokeText(const TextBlob* textBlob, const Stroke& stroke);

  virtual void clear() = 0;

  /**
   * Uploads the pixel data to a texture, and returns the texture. The texture is cached, so
   * multiple calls to this method will return the same texture. If you don't want to update the
   * texture, you can call this method once and use the texture returned before releasing the mask.
   */
  virtual std::shared_ptr<Texture> updateTexture(Context* context) = 0;

  std::shared_ptr<tgfx::Image> makeImage(Context* context);

 protected:
  int _width = 0;
  int _height = 0;
  Matrix matrix = Matrix::I();

  Mask(int width, int height) : _width(width), _height(height) {
  }
};
}  // namespace tgfx
