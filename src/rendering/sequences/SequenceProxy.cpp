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

#include "SequenceProxy.h"

namespace pag {
SequenceProxy::SequenceProxy(int width, int height, Sequence* sequence, Frame targetFrame)
    : TextureProxy(width, height), sequence(sequence), targetFrame(targetFrame) {
}

void SequenceProxy::prepare(RenderCache* cache) const {
  static_cast<RenderCache*>(cache)->prepareSequence(sequence, targetFrame);
}

std::shared_ptr<tgfx::Texture> SequenceProxy::getTexture(RenderCache* cache) const {
  return static_cast<RenderCache*>(cache)->getSequenceFrame(sequence, targetFrame);
}
}  // namespace pag
