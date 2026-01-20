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

#include <emscripten/bind.h>
#include "tgfx/gpu/Recording.h"
#include "tgfx/gpu/opengl/webgl/WebGLWindow.h"
#include "tgfx/layers/DisplayList.h"
#include "pagx/layers/LayerBuilder.h"

namespace pagx {

class PAGXView {
 public:
  PAGXView(const std::string& canvasID);

  void registerFonts(const emscripten::val& fontVal, const emscripten::val& emojiFontVal);

  void loadPAGX(const emscripten::val& pagxData);

  void updateSize();

  void updateZoomScaleAndOffset(float zoom, float offsetX, float offsetY);

  void draw();

  float contentWidth() const {
    return pagxWidth;
  }

  float contentHeight() const {
    return pagxHeight;
  }

 private:
  void applyCenteringTransform();

  std::string canvasID = {};
  std::shared_ptr<tgfx::Window> window = nullptr;
  tgfx::DisplayList displayList = {};
  std::shared_ptr<tgfx::Layer> contentLayer = nullptr;
  std::unique_ptr<tgfx::Recording> lastRecording = nullptr;
  int lastSurfaceWidth = 0;
  int lastSurfaceHeight = 0;
  bool presentImmediately = true;
  float pagxWidth = 0.0f;
  float pagxHeight = 0.0f;
};

}  // namespace pagx
