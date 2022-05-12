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

#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Mesh.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/RGBAAALayout.h"
#include "tgfx/gpu/Paint.h"
#include "tgfx/gpu/Texture.h"

namespace tgfx {
class Surface;

class SurfaceOptions;

struct CanvasState;

/**
 * Canvas provides an interface for drawing, and how the drawing is clipped and transformed. Canvas
 * contains a stack of opacity, blend mode, matrix and clip values. Each Canvas draw call transforms
 * the geometry of the object by the concatenation of all matrix values in the stack. The
 * transformed geometry is clipped by the intersection of all of clip values in the stack.
 */
class Canvas {
 public:
  explicit Canvas(Surface* surface);

  virtual ~Canvas() = default;

  /**
   * Retrieves the context associated with this Surface.
   */
  Context* getContext() const;

  /**
   * Returns the Surface this canvas draws into.
   */
  Surface* getSurface() const;

  /**
   * Returns SurfaceOptions associated with this canvas.
   */
  const SurfaceOptions* surfaceOptions() const;

  /**
   * Saves alpha, blend mode, matrix and clip. Calling restore() discards changes to them, restoring
   * them to their state when save() was called. Saved Canvas state is put on a stack, multiple
   * calls to save() should be balance by an equal number of calls to restore().
   */
  void save();

  /**
   * Removes changes to alpha, blend mode, matrix and clips since Canvas state was last saved. The
   * state is removed from the stack. Does nothing if the stack is empty.
   */
  void restore();

  /**
   * Returns the current total matrix.
   */
  Matrix getMatrix() const;

  /**
   * Replaces transformation with specified matrix. Unlike concat(), any prior matrix state is
   * overwritten.
   * @param matrix  matrix to copy, replacing existing Matrix
   */
  void setMatrix(const Matrix& matrix);

  /**
   * Sets Matrix to the identity matrix. Any prior matrix state is overwritten.
   */
  void resetMatrix();

  /**
   * Replaces Matrix with matrix premultiplied with existing Matrix. This has the effect of
   * transforming the drawn geometry by matrix, before transforming the result with existing Matrix.
   * @param matrix  matrix to premultiply with existing Matrix
   */
  void concat(const Matrix& matrix);

  /**
   * Returns the current global alpha.
   */
  float getAlpha() const;

  /**
   * Replaces the global alpha with specified newAlpha.
   */
  void setAlpha(float newAlpha);

  /**
   * Returns the current global blend mode.
   */
  BlendMode getBlendMode() const;

  /**
   * Replaces the global blend mode with specified new blend mode.
   */
  void setBlendMode(BlendMode blendMode);

  /**
   * Returns the current total clip.
   */
  Path getTotalClip() const;

  /**
   * Replaces clip with the intersection of clip and path. The path is transformed by Matrix before
   * it is combined with clip.
   */
  void clipPath(const Path& path);

  /**
   * Replacing all pixels with transparent color.
   */
  virtual void clear() = 0;

  /**
   * Draws a rectangle with specified paint, using current current alpha, blend mode, clip and
   * matrix.
   */
  void drawRect(const Rect& rect, const Paint& paint);

  /**
   * Draws a mask with specified paint, using current current alpha, blend mode, clip and
   * matrix.
   */
  virtual void drawMask(const Texture* mask, const Paint& paint) = 0;

  /**
   * Draws a Texture, with its top-left corner at (0, 0), using a mask texture and current alpha,
   * blend mode, clip and matrix. The mask texture has the same position and size with the texture.
   */
  virtual void drawTexture(const Texture* texture, const Texture* mask, bool inverted) = 0;

  /**
   *  Draws a RGBAAA layout Texture, with its top-left corner at (0, 0), using current alpha, blend
   *  mode, clip and matrix.
   */
  virtual void drawTexture(const Texture* texture, const RGBAAALayout* layout) = 0;

  /**
   *  Draws a Texture, with its top-left corner at (0, 0), using current alpha, blend mode, clip and
   *  Matrix.
   */
  void drawTexture(const Texture* texture);

  /**
   *  Draws a Texture, with its top-left corner at (0, 0), using current alpha, blend mode, clip and
   *  matrix premultiplied with existing Matrix.
   */
  void drawTexture(const Texture* texture, const Matrix& matrix);

  /**
   * Draws a path with using current clip, matrix and specified paint.
   */
  virtual void drawPath(const Path& path, const Paint& paint) = 0;

  /**
   * Draws a mesh with using current clip, matrix and specified paint.
   */
  virtual void drawMesh(const Mesh* mesh, const Paint& paint) = 0;

  /**
   * Draw array of glyphs with specified font, using current alpha, blend mode, clip and Matrix.
   */
  virtual void drawGlyphs(const GlyphID glyphIDs[], const Point positions[], size_t glyphCount,
                          const Font& font, const Paint& paint) = 0;

  // TODO(pengweilv): Support blend mode, atlas as source, colors as destination, colors can be
  //  nullptr.
  virtual void drawAtlas(const Texture* atlas, const Matrix matrix[], const Rect tex[],
                         const Color colors[], size_t count) = 0;

  /**
   * Triggers the immediate execution of all pending draw operations.
   */
  virtual void flush() {
  }

 protected:
  Surface* surface = nullptr;
  std::shared_ptr<CanvasState> state = nullptr;

  virtual void onSave() = 0;
  virtual void onRestore() = 0;
  virtual void onSetMatrix(const Matrix& matrix) = 0;
  virtual void onClipPath(const Path& path) = 0;

 private:
  std::vector<std::shared_ptr<CanvasState>> savedStateList = {};
};
}  // namespace tgfx
