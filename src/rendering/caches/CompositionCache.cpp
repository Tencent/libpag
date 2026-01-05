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

#include "CompositionCache.h"
#include "rendering/renderers/CompositionRenderer.h"

namespace pag {

CompositionCache* CompositionCache::Get(Composition* composition) {
  std::lock_guard<std::mutex> autoLock(composition->locker);
  if (composition->cache == nullptr) {
    composition->cache = new CompositionCache(composition);
  }
  return static_cast<CompositionCache*>(composition->cache);
}

CompositionCache::CompositionCache(Composition* composition) : composition(composition) {
}

std::shared_ptr<Graphic> CompositionCache::getContent(Frame contentFrame) {
  contentFrame = ConvertFrameByStaticTimeRanges(composition->staticTimeRanges, contentFrame);
  if (contentFrame >= composition->duration) {
    contentFrame = composition->duration - 1;
  }
  if (contentFrame < 0) {
    contentFrame = 0;
  }
  std::lock_guard<std::mutex> autoLock(locker);
  auto cache = frames[contentFrame];
  if (cache == nullptr) {
    cache = createContent(contentFrame);
    frames[contentFrame] = cache;
  }
  return cache;
}

std::shared_ptr<Graphic> CompositionCache::createContent(Frame compositionFrame) {
  if (composition->type() == CompositionType::Vector) {
    return RenderVectorComposition(static_cast<VectorComposition*>(composition), compositionFrame);
  }
  return RenderSequenceComposition(composition, compositionFrame);
}
}  // namespace pag
