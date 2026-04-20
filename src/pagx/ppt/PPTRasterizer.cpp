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

#include "pagx/ppt/PPTRasterizer.h"
#include "tgfx/core/Data.h"

namespace pagx {

PPTRasterizer::PPTRasterizer(PAGXDocument* doc, int dpi) : _doc(doc), _dpi(dpi) {
}

const LayerBuildResult& PPTRasterizer::ensureBuild() {
  if (!_buildResultReady) {
    _buildResult = LayerBuilder::BuildWithMap(_doc);
    _buildResultReady = true;
  }
  return _buildResult;
}

std::shared_ptr<tgfx::Layer> PPTRasterizer::getMappedLayer(const Layer* layer) {
  const auto& build = ensureBuild();
  auto it = build.layerMap.find(layer);
  if (it == build.layerMap.end()) {
    return nullptr;
  }
  return it->second;
}

std::shared_ptr<tgfx::Data> PPTRasterizer::renderLayer(const Layer* layer, Rect* outBounds) {
  const auto& build = ensureBuild();
  auto it = build.layerMap.find(layer);
  if (it == build.layerMap.end()) {
    return nullptr;
  }
  auto target = it->second;
  if (!target || !build.root) {
    return nullptr;
  }
  auto bounds = target->getBounds(build.root.get(), true);
  if (outBounds) {
    outBounds->x = bounds.left;
    outBounds->y = bounds.top;
    outBounds->width = bounds.width();
    outBounds->height = bounds.height();
  }
  return RenderMaskedLayer(&_gpu, build.root, target);
}

}  // namespace pagx
