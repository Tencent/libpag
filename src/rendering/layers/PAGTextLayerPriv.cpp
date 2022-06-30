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

#include "PAGTextLayerPriv.h"
#include "rendering/editing/TextReplacement.h"
#include "rendering/utils/LockGuard.h"

namespace pag {

std::shared_ptr<PAGTextLayerPriv> PAGTextLayerPriv::Make(std::shared_ptr<PAGTextLayer> layer) {
  return std::shared_ptr<PAGTextLayerPriv>(new PAGTextLayerPriv(layer));
}

PAGTextLayerPriv::PAGTextLayerPriv(std::shared_ptr<PAGTextLayer> pagTextLayer)
    : pagTextLayer(pagTextLayer) {
}

const Transform2D* PAGTextLayerPriv::transform() const {
  auto textLayer = pagTextLayer.lock();
  if (textLayer == nullptr) {
    return nullptr;
  }
  LockGuard autoLock(textLayer->rootLocker);
  return textLayer->layer->transform;
}

std::vector<TextAnimator*> PAGTextLayerPriv::textAnimators() {
  auto textLayer = pagTextLayer.lock();
  if (textLayer == nullptr) {
    return {};
  }
  LockGuard autoLock(textLayer->rootLocker);
  if (textLayer->replacement && textLayer->replacement->animators()) {
    return *textLayer->replacement->animators();
  }
  return static_cast<TextLayer*>(textLayer->layer)->animators;
}

void PAGTextLayerPriv::setAnimators(std::vector<TextAnimator*>* animators) {
  auto textLayer = pagTextLayer.lock();
  if (textLayer == nullptr) {
    return;
  }
  LockGuard autoLock(textLayer->rootLocker);
  if (!textLayer->replacement) {
    textLayer->replacement = new TextReplacement(textLayer.get());
  }
  textLayer->replacement->setAnimators(animators);
}

void PAGTextLayerPriv::setLayoutGlyphs(
    const std::vector<std::vector<std::shared_ptr<Glyph>>>& glyphs) {
  auto textLayer = pagTextLayer.lock();
  if (textLayer == nullptr) {
    return;
  }
  LockGuard autoLock(textLayer->rootLocker);
  if (!textLayer->replacement) {
    textLayer->replacement = new TextReplacement(textLayer.get());
  }
  textLayer->replacement->setLayoutGlyphs(glyphs);
}

void PAGTextLayerPriv::setJustification(Enum justification) {
  auto textLayer = pagTextLayer.lock();
  if (textLayer == nullptr) {
    return;
  }
  LockGuard autoLock(textLayer->rootLocker);
  textLayer->textDocumentForWrite()->justification = justification;
}

}  // namespace pag
