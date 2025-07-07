/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "Graphic.h"

namespace pag {
class Record;

/**
 * Recorder provides an interface for recording drawing commands made with Graphics, and creates a
 * new Graphic capturing the whole drawing commands which may be applied at a later time.
 */
class Recorder {
 public:
  /**
   * Returns the current total matrix.
   */
  tgfx::Matrix getMatrix() const;

  /**
   * Replaces transformation with specified matrix. Unlike concat(), any prior matrix state is
   * overwritten.
   * @param matrix  matrix to copy, replacing existing Matrix
   */
  void setMatrix(const tgfx::Matrix& matrix);

  /**
   * Replaces transformation with specified matrix premultiplied with existing Matrix. This has the
   * effect of transforming the drawn geometry by matrix, before transforming the result with
   * existing Matrix.
   * @param matrix  matrix to premultiply with existing Matrix
   */
  void concat(const tgfx::Matrix& matrix);

  /**
   * Saves matrix, and allocates a layer for subsequent drawing. Calling restore() discards changes
   * to matrix, and blends layer with the clip rect onto prior layer. Saved Recorder state is put on
   * a stack, multiple calls to saveLayer(), saveClip() and save() should be balance by an equal
   * number of calls to restore().
   */
  void saveClip(float x, float y, float width, float height);

  /**
   * Saves matrix, and allocates a layer for subsequent drawing. Calling restore() discards changes
   * to matrix, and blends layer with the clip path onto prior layer. Saved Recorder state is put on
   * a stack, multiple calls to saveLayer(), saveClip() and save() should be balance by an equal
   * number of calls to restore().
   */
  void saveClip(const tgfx::Path& path);

  /**
   * Saves matrix, and allocates a layer for subsequent drawing. Calling restore() discards changes
   * to matrix, and blends layer with specified styles onto prior layer. Saved Recorder state is put
   * on a stack, multiple calls to saveLayer(), saveClip() and save() should be balance by an equal
   * number of calls to restore().
   */
  void saveLayer(float alpha, tgfx::BlendMode blendMode);

  /**
   * Saves matrix, and allocates a layer for subsequent drawing. Calling restore() discards changes
   * to matrix, and blends layer with the modifier onto prior layer. Saved Recorder state is put on
   * a stack, multiple calls to saveLayer(), saveClip() and save() should be balance by an equal
   * number of calls to restore().
   */
  void saveLayer(std::shared_ptr<Modifier> modifier);

  /**
   * Saves matrix. Calling restore() discards changes to them, restoring the matrix to its state
   * when save() was called. Saved Recorder state is put on a stack, multiple calls to saveLayer(),
   * saveClip() and save()  should be balance by an equal number of calls to restore().
   */
  void save();

  /**
   * Removes changes to matrix and clips since Recorder state was last saved. The state is removed
   * from the stack. Does nothing if the stack is empty.
   */
  void restore();

  /**
   * Returns the number of saved states, the save count of a new recorder is 0.
   */
  size_t getSaveCount() const;

  /**
   * Restores state to specified saveCount. Does nothing if saveCount is greater than state stack
   * count. Restores state to initial values if saveCount is equal to 0.
   */
  void restoreToCount(size_t saveCount);

  /**
   * Draws a graphic using current clip and matrix.
   */
  void drawGraphic(std::shared_ptr<Graphic> graphic);

  /**
   * Draws a graphic using current clip and extra matrix.
   */
  void drawGraphic(std::shared_ptr<Graphic> graphic, const tgfx::Matrix& matrix);

  /**
   * Returns a Graphic capturing recorder contents. Subsequent drawing to recorder contents are not
   * captured. Returns nullptr if the contents are empty.
   */
  std::shared_ptr<Graphic> makeGraphic();

 private:
  std::vector<std::shared_ptr<Graphic>> rootContents = {};
  int layerIndex = 0;
  tgfx::Matrix matrix = tgfx::Matrix::I();
  std::vector<std::shared_ptr<Graphic>> layerContents = {};
  std::vector<std::shared_ptr<Record>> records = {};
};
}  // namespace pag
