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
SequenceProxy::SequenceProxy(int width, int height, std::unique_ptr<SequenceReaderFactory> factory,
                             Frame targetFrame)
    : TextureProxy(width, height), factory(std::move(factory)), targetFrame(targetFrame) {
}

void SequenceProxy::prepare(RenderCache* cache) const {
  static_cast<RenderCache*>(cache)->prepareSequenceReader(factory.get(), targetFrame);
}

std::shared_ptr<tgfx::Texture> SequenceProxy::getTexture(RenderCache* cache) const {
  auto reader = static_cast<RenderCache*>(cache)->getSequenceReader(factory.get());
  if (reader) {
    return reader->readTexture(targetFrame, static_cast<RenderCache*>(cache));
  }
  return nullptr;
}
}  // namespace pag
