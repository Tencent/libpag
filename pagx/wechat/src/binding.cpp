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

#include <emscripten/bind.h>
#include "PAGXView.h"

using namespace emscripten;

using namespace emscripten;
using namespace pagx;

EMSCRIPTEN_BINDINGS(PAGXView) {
  class_<PAGXView>("PAGXView")
      .smart_ptr<std::shared_ptr<PAGXView>>("PAGXView")
      .class_function("MakeFrom", &PAGXView::MakeFrom)
      .function("width", &PAGXView::width)
      .function("height", &PAGXView::height)
      .function("loadPAGX", &PAGXView::loadPAGX)
      .function("updateSize", &PAGXView::updateSize)
      .function("updateZoomScaleAndOffset", &PAGXView::updateZoomScaleAndOffset)
      .function("onZoomEnd", &PAGXView::onZoomEnd)
      .function("draw", &PAGXView::draw)
      .function("contentWidth", &PAGXView::contentWidth)
      .function("contentHeight", &PAGXView::contentHeight)
      .function("setPerformanceAdaptationEnabled", &PAGXView::setPerformanceAdaptationEnabled)
      .function("setSlowFrameThreshold", &PAGXView::setSlowFrameThreshold)
      .function("setRecoveryWindow", &PAGXView::setRecoveryWindow)
      .function("isLastFrameSlow", &PAGXView::isLastFrameSlow)
      .function("getAverageFrameTime", &PAGXView::getAverageFrameTime);
}
