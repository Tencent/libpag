/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/Image.h"
#include "tgfx/core/ImageBuffer.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/Stroke.h"
#include "tgfx/core/TextBlob.h"

namespace tgfx {
class ImageStream;

/**
 * Mask is a utility that can take an image described in a vector graphics format (paths, glyphs)
 * and convert it into a raster image that can be used as a drawing mask. Mask is not thread safe,
 * do not use it across multiple threads.
 */
class Mask {
 public:
  /**
   * Creates a new Mask and tries to allocate its pixel memory by the specified width and height.
   * If tryHardware is true and there is hardware buffer support on the current platform,
   * a hardware-backed pixel buffer is allocated. Otherwise, a raster pixel buffer is allocated.
   * Returns nullptr if allocation fails.
   */
  static std::shared_ptr<Mask> Make(int width, int height, bool tryHardware = true);

  virtual ~Mask() = default;

  /**
   * Returns the width of the Mask.
   */
  virtual int width() const = 0;

  /**
   * Returns the height of the Mask.
   */
  virtual int height() const = 0;

  /**
   * Returns true if the Mask is backed by a platform-specified hardware buffer.
   */
  virtual bool isHardwareBacked() const = 0;

  /**
   * Returns the current total matrix.
   */
  Matrix getMatrix() const {
    return matrix;
  }

  /**
   * Replaces transformation with the specified matrix.
   */
  void setMatrix(const Matrix& m) {
    matrix = m;
  }

  /**
   * Replaces the current Matrix with matrix premultiplied with the existing one. This has the
   * effect of transforming the filling geometry by matrix, before transforming the result with
   * the existing Matrix.
   */
  void concat(const Matrix& m) {
    matrix.preConcat(m);
  }

  /**
   * Writes the fills or outlines of the given Path to the Mask，with its top-left corner at (0, 0),
   * using the current Matrix.
   */
  void fillPath(const Path& path, const Stroke* stroke = nullptr);

  /**
   * Writes the fills or outlines of the given TextBlob to the Mask，with its top-left corner at
   * (0, 0), using the current Matrix. Returns false if the associated typeface has color glyphs and
   * leaves the Mask unchanged.
   */
  bool fillText(const TextBlob* textBlob, const Stroke* stroke = nullptr);

  /**
   * Replaces all pixel values with transparent colors.
   */
  virtual void clear() = 0;

  /**
   * Returns an ImageBuffer object capturing the pixels in the Mask. Subsequent writing of the Mask
   * will not be captured. Instead, the Mask will copy its pixels to a new memory buffer if there is
   * a subsequent writing call to the Mask while the returned ImageBuffer is still alive. If the
   * Mask is modified frequently, create an ImageReader from the Mask instead, which allows you to
   * continuously read the latest content from the Mask with minimal memory copying. Returns nullptr
   * if the Mask is empty.
   */
  virtual std::shared_ptr<ImageBuffer> makeBuffer() const = 0;

 protected:
  virtual std::shared_ptr<ImageStream> getImageStream() const = 0;

  virtual void onFillPath(const Path& path, const Matrix& matrix) = 0;

  virtual bool onFillText(const TextBlob* textBlob, const Stroke* stroke, const Matrix& matrix);

 private:
  Matrix matrix = Matrix::I();

  friend class ImageReader;
};
}  // namespace tgfx
