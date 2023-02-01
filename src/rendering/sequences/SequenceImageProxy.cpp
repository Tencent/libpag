/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "SequenceImageProxy.h"
#include "SequenceImage.h"
#include "rendering/caches/RenderCache.h"

namespace pag {
SequenceImageProxy::SequenceImageProxy(Sequence* sequence, Frame targetFrame)
    : sequence(sequence), targetFrame(targetFrame) {
}

void SequenceImageProxy::prepareImage(RenderCache* cache) const {
  auto composition = sequence->composition;
  if (composition->staticContent()) {
    // We treat sequences with static content as normal asset images.
    cache->prepareAssetImage(composition->uniqueID, this);
    return;
  }
  cache->prepareSequenceImage(sequence, targetFrame);
}

std::shared_ptr<tgfx::Image> SequenceImageProxy::getImage(RenderCache* cache) const {
  auto composition = sequence->composition;
  if (composition->staticContent()) {
    return cache->getAssetImage(composition->uniqueID, this);
  }
  return cache->getSequenceImage(sequence, targetFrame);
}

std::shared_ptr<tgfx::Image> SequenceImageProxy::makeImage(RenderCache* cache) const {
  auto composition = sequence->composition;
  if (!composition->staticContent()) {
    return nullptr;
  }
  auto file = cache->getFileByAssetID(composition->uniqueID);
  return SequenceImage::MakeStatic(std::move(file), sequence);
}
}  // namespace pag
