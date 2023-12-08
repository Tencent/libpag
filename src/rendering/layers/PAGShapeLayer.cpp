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

#include <rendering/utils/LockGuard.h>
#include <rendering/renderers/ShapeRenderer.h>
#include "pag/pag.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/layers/PAGStage.h"

namespace pag {
PAGShapeLayer::PAGShapeLayer(std::shared_ptr<pag::File> file, ShapeLayer* layer)
    : PAGLayer(std::move(file), layer) {
}

std::shared_ptr<Color> PAGShapeLayer::getTintColor() const {
    LockGuard autoLock(rootLocker);
    auto shapeLayer = static_cast<ShapeLayer*>(layer);
    return shapeLayer->getTintColor();
}

void PAGShapeLayer::setTintColor(pag::Color value) {
    LockGuard autoLock(rootLocker);

    auto shapeLayer = static_cast<ShapeLayer*>(layer);
    shapeLayer->setTintColor(value);
}

void PAGShapeLayer::clearTintColor() {
    LockGuard autoLock(rootLocker);

    auto shapeLayer = static_cast<ShapeLayer*>(layer);
    shapeLayer->clearTintColor();
}

}  // namespace pag