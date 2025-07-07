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

#include "TrackMatteRenderer.h"
#include "base/utils/TGFXCast.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/caches/TextContent.h"
#include "rendering/renderers/LayerRenderer.h"

namespace pag {
static std::shared_ptr<Graphic> RenderColorGlyphs(TextLayer* layer, Frame layerFrame,
                                                  TextContent* textContent = nullptr,
                                                  Transform* extraTransform = nullptr) {
  if (extraTransform && !extraTransform->visible()) {
    return nullptr;
  }
  auto layerCache = LayerCache::Get(layer);
  auto contentFrame = layerFrame - layer->startTime;
  auto content =
      textContent ? textContent : static_cast<TextContent*>(layerCache->getContent(contentFrame));
  if (content->colorGlyphs == nullptr) {
    return nullptr;
  }
  auto layerTransform = layerCache->getTransform(contentFrame);
  Recorder recorder = {};
  recorder.saveLayer(layerTransform->alpha, ToTGFX(layer->blendMode));
  if (extraTransform) {
    recorder.concat(extraTransform->matrix);
  }
  recorder.concat(layerTransform->matrix);
  auto masks = layerCache->getMasks(contentFrame);
  if (masks) {
    recorder.saveClip(*masks);
  }
  recorder.drawGraphic(content->colorGlyphs);
  if (masks) {
    recorder.restore();
  }
  recorder.restore();
  return recorder.makeGraphic();
}

static std::shared_ptr<Modifier> MakeMaskModifier(std::shared_ptr<Graphic> content,
                                                  TrackMatteType trackMatteType) {
  auto inverted = (trackMatteType == TrackMatteType::AlphaInverted ||
                   trackMatteType == TrackMatteType::LumaInverted);
  auto useLuma =
      (trackMatteType == TrackMatteType::Luma || trackMatteType == TrackMatteType::LumaInverted);
  return Modifier::MakeMask(std::move(content), inverted, useLuma);
}

std::unique_ptr<TrackMatte> TrackMatteRenderer::Make(PAGLayer* trackMatteOwner) {
  if (trackMatteOwner == nullptr || trackMatteOwner->_trackMatteLayer == nullptr) {
    return nullptr;
  }
  auto trackMatteLayer = trackMatteOwner->_trackMatteLayer.get();
  auto trackMatteType = trackMatteOwner->layer->trackMatteType;
  auto layerFrame = trackMatteLayer->contentFrame + trackMatteLayer->layer->startTime;
  std::shared_ptr<FilterModifier> filterModifier = nullptr;
  if (!trackMatteLayer->cacheFilters()) {
    filterModifier = FilterModifier::Make(trackMatteLayer);
  }
  Recorder recorder = {};
  Transform extraTransform = {ToTGFX(trackMatteLayer->layerMatrix), trackMatteLayer->layerAlpha};
  LayerRenderer::DrawLayer(&recorder, trackMatteLayer->layer, layerFrame, filterModifier, nullptr,
                           trackMatteLayer, &extraTransform);
  auto content = recorder.makeGraphic();
  auto trackMatte = std::make_unique<TrackMatte>();
  trackMatte->modifier = MakeMaskModifier(content, trackMatteType);
  if (trackMatte->modifier == nullptr) {
    return nullptr;
  }
  if (trackMatteLayer->layerType() == LayerType::Text) {
    auto textContent = static_cast<TextContent*>(trackMatteLayer->getContent());
    trackMatte->colorGlyphs = RenderColorGlyphs(static_cast<TextLayer*>(trackMatteLayer->layer),
                                                layerFrame, textContent, &extraTransform);
  }
  return trackMatte;
}

std::unique_ptr<TrackMatte> TrackMatteRenderer::Make(Layer* trackMatteOwner, Frame layerFrame) {
  if (trackMatteOwner == nullptr || trackMatteOwner->trackMatteLayer == nullptr) {
    return nullptr;
  }
  auto trackMatteLayer = trackMatteOwner->trackMatteLayer;
  auto trackMatteType = trackMatteOwner->trackMatteType;
  auto filterModifier = FilterModifier::Make(trackMatteLayer, layerFrame);
  Recorder recorder = {};
  LayerRenderer::DrawLayer(&recorder, trackMatteLayer, layerFrame, filterModifier, nullptr);
  auto content = recorder.makeGraphic();
  auto trackMatte = std::make_unique<TrackMatte>();
  trackMatte->modifier = MakeMaskModifier(content, trackMatteType);
  if (trackMatte->modifier == nullptr) {
    return nullptr;
  }
  if (trackMatteLayer->type() == LayerType::Text) {
    trackMatte->colorGlyphs =
        RenderColorGlyphs(static_cast<TextLayer*>(trackMatteLayer), layerFrame);
  }
  return trackMatte;
}
}  // namespace pag
