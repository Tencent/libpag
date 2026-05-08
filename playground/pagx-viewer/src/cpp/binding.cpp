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
#include "PAGXView.h"

using namespace emscripten;

EMSCRIPTEN_BINDINGS(PAGXPlayground) {
  class_<pagx::PAGXView>("_PAGXView")
      .smart_ptr<std::shared_ptr<pagx::PAGXView>>("_PAGXView")
      .class_function("_MakeFrom",
                      // Lambda is required here: optional_override() only accepts a callable
                      // with an emscripten-compatible signature. A named function would also work
                      // but adds another symbol for a one-off adapter.
                      optional_override([](const std::string& canvasID) {
                        if (canvasID.empty()) {
                          return std::shared_ptr<pagx::PAGXView>(nullptr);
                        }
                        return std::make_shared<pagx::PAGXView>(canvasID);
                      }))
      .function("_registerFonts", &pagx::PAGXView::registerFonts)
      .function("_loadPAGX", &pagx::PAGXView::loadPAGX)
      .function("_parsePAGX", &pagx::PAGXView::parsePAGX)
      .function("_getExternalFilePaths", &pagx::PAGXView::getExternalFilePaths)
      .function("_loadFileData", &pagx::PAGXView::loadFileData)
      .function("_buildLayers", &pagx::PAGXView::buildLayers)
      .function("_updateSize", &pagx::PAGXView::updateSize)
      .function("_updateZoomScaleAndOffset", &pagx::PAGXView::updateZoomScaleAndOffset)
      .function("_setBackgroundColor", &pagx::PAGXView::setBackgroundColor)
      .function("_clearBackgroundColor", &pagx::PAGXView::clearBackgroundColor)
      .function("_draw", &pagx::PAGXView::draw)
      .function("_contentWidth", &pagx::PAGXView::contentWidth)
      .function("_contentHeight", &pagx::PAGXView::contentHeight);
}
