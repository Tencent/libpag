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

#include "AsyncImageBuffer.h"

namespace tgfx {
std::shared_ptr<ImageBuffer> AsyncImageBuffer::MakeFrom(std::shared_ptr<ImageGenerator> generator,
                                                        bool tryHardware) {
  if (generator == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<ImageBuffer>(new AsyncImageBuffer(std::move(generator), tryHardware));
}

AsyncImageBuffer::AsyncImageBuffer(std::shared_ptr<ImageGenerator> generator, bool tryHardware)
    : ImageBuffer(generator->width(), generator->height()),
      generator(std::move(generator)),
      tryHardware(tryHardware) {
  task = Task::Make(this);
  task->run();
}

std::shared_ptr<Texture> AsyncImageBuffer::makeTexture(Context* context) const {
  task->wait();
  if (imageBuffer == nullptr) {
    return nullptr;
  }
  return imageBuffer->makeTexture(context);
}

std::shared_ptr<Texture> AsyncImageBuffer::makeMipMappedTexture(Context* context) const {
  task->wait();
  if (imageBuffer == nullptr) {
    return nullptr;
  }
  return imageBuffer->makeMipMappedTexture(context);
}

void AsyncImageBuffer::execute() {
  imageBuffer = generator->makeBuffer(tryHardware);
}
}  // namespace tgfx
