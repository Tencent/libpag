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

#include <emscripten/bind.h>
#include <string>
#include <vector>
#include "PAGXView.h"

using namespace emscripten;

EMSCRIPTEN_BINDINGS(PAGXPlayground) {
  register_vector<std::string>("StringVector");

  class_<pagx::PAGXView>("PAGXView")
      .smart_ptr<std::shared_ptr<pagx::PAGXView>>("PAGXView")
      .class_function("MakeFrom", optional_override([](const std::string& canvasID) {
                        if (canvasID.empty()) {
                          return std::shared_ptr<pagx::PAGXView>(nullptr);
                        }
                        return std::make_shared<pagx::PAGXView>(canvasID);
                      }))
      .function("registerFonts", &pagx::PAGXView::registerFonts)
      .function("loadPAGX", &pagx::PAGXView::loadPAGX)
      .function("parsePAGX", &pagx::PAGXView::parsePAGX)
      .function("getExternalFilePaths", &pagx::PAGXView::getExternalFilePaths)
      .function("loadFileData", &pagx::PAGXView::loadFileData)
      .function("buildLayers", &pagx::PAGXView::buildLayers)
      .function("updateSize", &pagx::PAGXView::updateSize)
      .function("updateZoomScaleAndOffset", &pagx::PAGXView::updateZoomScaleAndOffset)
      .function("draw", &pagx::PAGXView::draw)
      .function("contentWidth", &pagx::PAGXView::contentWidth)
      .function("contentHeight", &pagx::PAGXView::contentHeight);
}
