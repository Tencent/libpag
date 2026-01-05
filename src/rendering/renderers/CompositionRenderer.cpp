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

#include "CompositionRenderer.h"
#include "LayerRenderer.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/graphics/Picture.h"
#include "rendering/graphics/Recorder.h"
#include "rendering/sequences/SequenceImageProxy.h"
#include "rendering/sequences/SequenceInfo.h"

namespace pag {
std::shared_ptr<Graphic> RenderVectorComposition(VectorComposition* composition,
                                                 Frame compositionFrame) {
  Recorder recorder = {};
  recorder.saveClip(0, 0, static_cast<float>(composition->width),
                    static_cast<float>(composition->height));
  auto& layers = composition->layers;
  // The index order of Layers in File is different from PAGLayers.
  for (int i = static_cast<int>(layers.size()) - 1; i >= 0; i--) {
    auto childLayer = layers[i];
    if (!childLayer->isActive) {
      continue;
    }
    auto layerCache = LayerCache::Get(childLayer);
    auto filterModifier =
        layerCache->cacheFilters() ? nullptr : FilterModifier::Make(childLayer, compositionFrame);
    auto trackMatte = TrackMatteRenderer::Make(childLayer, compositionFrame);
    LayerRenderer::DrawLayer(&recorder, childLayer, compositionFrame, filterModifier,
                             trackMatte.get());
  }
  recorder.restore();
  auto graphic = recorder.makeGraphic();
  if (layers.size() > 1 && composition->staticContent() && !composition->hasImageContent()) {
    // 仅当子项列表只存在矢量内容并图层数量大于 1 时才包装一个 Image，避免重复的 Image 包装。
    graphic = Picture::MakeFrom(composition->uniqueID, graphic);
  }
  return graphic;
}

std::shared_ptr<Graphic> RenderSequenceComposition(Composition* composition,
                                                   Frame compositionFrame) {
  auto sequence = Sequence::Get(composition);
  if (sequence == nullptr) {
    return nullptr;
  }
  auto sequenceFrame = sequence->toSequenceFrame(compositionFrame);
  auto info = SequenceInfo::Make(sequence);
  auto proxy = std::make_shared<SequenceImageProxy>(info, sequenceFrame);
  auto graphic = Picture::MakeFrom(sequence->composition->uniqueID, std::move(proxy));
  auto scaleX = static_cast<float>(composition->width) / static_cast<float>(sequence->width);
  auto scaleY = static_cast<float>(composition->height) / static_cast<float>(sequence->height);
  return Graphic::MakeCompose(graphic, tgfx::Matrix::MakeScale(scaleX, scaleY));
}
}  // namespace pag
