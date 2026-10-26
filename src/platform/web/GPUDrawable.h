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

#include <emscripten/val.h>
#include "rendering/drawables/Drawable.h"
#include "tgfx/gpu/Window.h"

namespace pag {
class GPUDrawable : public Drawable {
 public:
  static std::shared_ptr<GPUDrawable> FromCanvasID(const std::string& canvasID);

  /**
   * Creates a GPUDrawable from a canvas object (HTMLCanvasElement or OffscreenCanvas) held by
   * the calling thread. Used for canvases that have no DOM id to build a selector from.
   */
  static std::shared_ptr<GPUDrawable> FromCanvas(emscripten::val canvas);

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
  explicit GPUDrawable(emscripten::val canvas);

  std::string _canvasID;
  emscripten::val _canvas = emscripten::val::undefined();
  int _width = 0;
  int _height = 0;
  std::shared_ptr<tgfx::Window> _window = nullptr;
};
}  // namespace pag
