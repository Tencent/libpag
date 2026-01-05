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

#include <base/utils/TimeUtil.h>
#include "base/utils/TGFXCast.h"
#include "pag/pag.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/graphics/Shape.h"
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
  layer->transform = transform.release();
  layer->width = width;
  layer->height = height;
  layer->solidColor = solidColor;
  layer->duration = TimeToFrame(duration, 60);

  auto pagSolidLayer = std::make_shared<PAGSolidLayer>(nullptr, layer);
  pagSolidLayer->emptySolidLayer = layer;
  pagSolidLayer->weakThis = pagSolidLayer;
  return pagSolidLayer;
}

PAGSolidLayer::PAGSolidLayer(std::shared_ptr<pag::File> file, SolidLayer* layer)
    : PAGLayer(file, layer), _solidColor(layer->solidColor) {
}

PAGSolidLayer::~PAGSolidLayer() {
  delete replacement;
  delete emptySolidLayer;
}

Content* PAGSolidLayer::getContent() {
  if (replacement != nullptr) {
    return replacement;
  }
  return layerCache->getContent(contentFrame);
}

bool PAGSolidLayer::contentModified() const {
  return replacement != nullptr;
}

Color PAGSolidLayer::solidColor() {
  LockGuard autoLock(rootLocker);
  return _solidColor;
}

void PAGSolidLayer::setSolidColor(const pag::Color& value) {
  LockGuard autoLock(rootLocker);
  if (_solidColor == value) {
    return;
  }
  _solidColor = value;
  if (replacement != nullptr) {
    delete replacement;
    replacement = nullptr;
  }
  auto solidLayer = static_cast<SolidLayer*>(layer);
  if (solidLayer->solidColor != _solidColor) {
    tgfx::Path path = {};
    path.addRect(0, 0, solidLayer->width, solidLayer->height);
    auto solid = Shape::MakeFrom(uniqueID(), path, ToTGFX(_solidColor));
    replacement = new GraphicContent(solid);
  }
  notifyModified(true);
  invalidateCacheScale();
}

}  // namespace pag
