/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <base/utils/TimeUtil.h>
#include "base/utils/TGFXCast.h"
#include "pag/pag.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/editing/ColorReplacement.h"
#include "rendering/editing/ReplacementHolder.h"
#include "rendering/layers/PAGStage.h"
#include "rendering/utils/LockGuard.h"

namespace pag {
std::shared_ptr<PAGSolidLayer> PAGSolidLayer::Make(int64_t duration, int32_t width, int32_t height,
                                                   Color solidColor, Opacity opacity) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  auto layer = new SolidLayer();
  auto transform = Transform2D::MakeDefault();
  transform->opacity->value = opacity;
  layer->transform = transform;
  layer->width = width;
  layer->height = height;
  layer->solidColor = solidColor;
  layer->duration = TimeToFrame(duration, 60);

  auto pagSolidLayer = std::make_shared<PAGSolidLayer>(nullptr, layer);
  pagSolidLayer->emptySolidLayer = layer;
  pagSolidLayer->weakThis = pagSolidLayer;
  pagSolidLayer->_editableIndex = 0;
  pagSolidLayer->colorHolder = std::make_shared<ReplacementHolder<Color>>();
  pagSolidLayer->colorHolder->addLayer(pagSolidLayer.get());
  pagSolidLayer->replacement = new ColorReplacement(
      pagSolidLayer.get(), pagSolidLayer->colorHolder.get(), pagSolidLayer->_editableIndex);
  return pagSolidLayer;
}

PAGSolidLayer::PAGSolidLayer(std::shared_ptr<pag::File> file, SolidLayer* layer)
    : PAGLayer(file, layer) {
}

PAGSolidLayer::~PAGSolidLayer() {
  delete replacement;
  delete emptySolidLayer;
}

Content* PAGSolidLayer::getContent() {
  if (contentModified()) {
    return replacement->getContent(contentFrame);
  }
  return layerCache->getContent(contentFrame);
}

bool PAGSolidLayer::contentModified() const {
  return colorHolder && colorHolder->hasReplacement(_editableIndex);
}

void PAGSolidLayer::onAddToRootFile(PAGFile* pagFile) {
  PAGLayer::onAddToRootFile(pagFile);
  colorHolder = pagFile->solidColorHolder;
  colorHolder->addLayer(this);
  replacement = new ColorReplacement(this, colorHolder.get(), _editableIndex);
}

void PAGSolidLayer::onRemoveFromRootFile() {
  PAGLayer::onRemoveFromRootFile();
  colorHolder->removeLayer(this);
  delete replacement;
  replacement = nullptr;
  colorHolder = nullptr;
}

Color PAGSolidLayer::solidColor() {
  LockGuard autoLock(rootLocker);
  return solidColorInternal();
}

void PAGSolidLayer::setSolidColor(const pag::Color& value) {
  LockGuard autoLock(rootLocker);
  auto currentColor = solidColorInternal();
  if (currentColor == value) {
    return;
  }
  if (colorHolder == nullptr) {
    return;
  }
  replacement->clearCache();
  colorHolder->setReplacement(_editableIndex, std::make_shared<pag::Color>(value), this);
  notifyReferenceLayers();
}

Color PAGSolidLayer::solidColorInternal() {
  return contentModified() ? replacement->solidColor()
                           : static_cast<SolidLayer*>(layer)->solidColor;
}

void PAGSolidLayer::notifyReferenceLayers() {
  std::vector<PAGSolidLayer*> solidLayers = {};
  if (rootFile) {
    auto layers = rootFile->getLayersByEditableIndexInternal(_editableIndex, LayerType::Solid);
    for (auto& pagLayer : layers) {
      solidLayers.push_back(static_cast<PAGSolidLayer*>(pagLayer.get()));
    }
  } else {
    solidLayers.push_back(this);
  }
  std::for_each(solidLayers.begin(), solidLayers.end(), [](PAGSolidLayer* solidLayer) {
    solidLayer->notifyModified(true);
    solidLayer->invalidateCacheScale();
  });
}

}  // namespace pag