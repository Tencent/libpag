/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
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

#include <string>
#include "pagx/runtime/Drawable.h"
#include "tgfx/gpu/opengl/webgl/WebGLWindow.h"

namespace pagx {

/**
 * CanvasDrawable adapts a browser canvas (selected by its element id) into the pagx Drawable
 * protocol. The underlying WebGL device and surface are created lazily on first access and
 * tracked for canvas resize via updateSize().
 */
class CanvasDrawable : public Drawable {
 public:
  /**
   * Creates a CanvasDrawable bound to the given canvas element id. Returns nullptr if canvasID
   * is empty.
   */
  static std::shared_ptr<CanvasDrawable> FromCanvasID(const std::string& canvasID);

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  std::shared_ptr<tgfx::Device> getDevice() override;
  std::shared_ptr<tgfx::Surface> getSurface(tgfx::Context* context) override;
  void updateSize() override;
  void present(tgfx::Context* context) override;

 private:
  explicit CanvasDrawable(std::string canvasID);

  std::string canvasID;
  int _width = 0;
  int _height = 0;
  std::shared_ptr<tgfx::WebGLWindow> window = nullptr;
};

}  // namespace pagx
