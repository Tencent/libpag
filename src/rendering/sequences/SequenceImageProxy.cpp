/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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
#include <utility>
#include "SequenceInfo.h"
#include "rendering/caches/RenderCache.h"

namespace pag {
SequenceImageProxy::SequenceImageProxy(std::shared_ptr<SequenceInfo> sequence, Frame targetFrame)
    : sequence(std::move(sequence)), targetFrame(targetFrame) {
}

void SequenceImageProxy::prepareImage(RenderCache* cache) const {
  if (sequence->staticContent()) {
    // We treat sequences with static content as normal asset images.
    cache->prepareAssetImage(sequence->uniqueID(), this);
    return;
  }
  cache->prepareSequenceImage(sequence, targetFrame);
}

std::shared_ptr<tgfx::Image> SequenceImageProxy::getImage(RenderCache* cache) const {
  if (sequence->staticContent()) {
    return cache->getAssetImage(sequence->uniqueID(), this);
  }
  return cache->getSequenceImage(sequence, targetFrame);
}

std::shared_ptr<tgfx::Image> SequenceImageProxy::makeImage(RenderCache* cache) const {
  if (!sequence->staticContent()) {
    return nullptr;
  }
  auto file = cache->getFileByAssetID(sequence->uniqueID());
  return sequence->makeStaticImage(std::move(file), cache->useDiskCache());
}
}  // namespace pag
