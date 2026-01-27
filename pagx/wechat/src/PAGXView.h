/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include <emscripten/bind.h>
#include "tgfx/gpu/Recording.h"
#include "tgfx/layers/DisplayList.h"
#include "pagx/LayerBuilder.h"

namespace pagx {

class PAGXView {
 public:
  /**
 * Creates a PAGXView instance for WeChat Mini Program rendering.
 * @param width The width of the canvas in pixels.
 * @param height The height of the canvas in pixels.
 * @return A shared pointer to the created PAGXView, or nullptr if creation fails.
 *
 * Note: Before calling this method, the JavaScript code must:
 * 1. Get the Canvas object from WeChat API
 * 2. Call canvas.getContext('webgl') to get WebGLRenderingContext
 * 3. Register the context via GL.registerContext(gl)
 */
  static std::shared_ptr<PAGXView> MakeFrom(int width, int height);


  PAGXView(std::shared_ptr<tgfx::Device> device, int width, int height);

  void loadPAGX(const emscripten::val& pagxData);

  /**
   * Updates the canvas size and recreates the surface.
   * @param width New width in pixels.
   * @param height New height in pixels.
   */
  void updateSize(int width, int height);

  void updateZoomScaleAndOffset(float zoom, float offsetX, float offsetY);

  bool draw();

  float contentWidth() const {
    return pagxWidth;
  }

  float contentHeight() const {
    return pagxHeight;
  }

  /**
 * Returns the width of the canvas in pixels.
 */
  int width() const;

  /**
   * Returns the height of the canvas in pixels.
   */
  int height() const;

 private:
  void applyCenteringTransform();

  std::shared_ptr<tgfx::Device> device = nullptr;
  std::shared_ptr<tgfx::Surface> surface = nullptr;
  tgfx::DisplayList displayList = {};
  std::shared_ptr<tgfx::Layer> contentLayer = nullptr;
  float pagxWidth = 0.0f;
  float pagxHeight = 0.0f;
  int _width = 0;
  int _height = 0;
};

}  // namespace pagx
