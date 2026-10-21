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

#include "rendering/drawables/Drawable.h"
#include "tgfx/gpu/Window.h"
#if defined(TGFX_USE_WEBGPU)
#include <emscripten/val.h>
#endif

namespace pag {
class GPUDrawable : public Drawable {
 public:
  static std::shared_ptr<GPUDrawable> FromCanvasID(const std::string& canvasID);

#if defined(TGFX_USE_WEBGPU)
  /**
   * Creates a GPUDrawable from a canvas object (HTMLCanvasElement or OffscreenCanvas) held by
   * the calling thread. Used for canvases that have no DOM id to build a selector from.
   */
  static std::shared_ptr<GPUDrawable> FromCanvas(emscripten::val canvas);
#endif

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  std::shared_ptr<tgfx::Device> getDevice() override;

  void updateSize() override;

  void present(tgfx::Context*) override {
  }

 protected:
  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

  void onFreeSurface() override;

 private:
  explicit GPUDrawable(std::string canvasID);
#if defined(TGFX_USE_WEBGPU)
  explicit GPUDrawable(emscripten::val canvas);
#endif

  std::string canvasID;
#if defined(TGFX_USE_WEBGPU)
  emscripten::val canvas = emscripten::val::undefined();
#endif
  int _width = 0;
  int _height = 0;
  std::shared_ptr<tgfx::Window> window = nullptr;
};
}  // namespace pag
