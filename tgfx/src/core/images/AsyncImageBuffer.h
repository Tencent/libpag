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

#pragma once

#include "core/utils/Task.h"
#include "tgfx/core/ImageGenerator.h"

namespace tgfx {
/**
 * A asynchronous ImageBuffer that wraps an ImageGenerator and schedules an asynchronous decoding
 * task immediately.
 */
class AsyncImageBuffer : public ImageBuffer, public Executor {
 public:
  static std::shared_ptr<ImageBuffer> MakeFrom(std::shared_ptr<ImageGenerator> generator);

  bool isAlphaOnly() const override {
    return generator->isAlphaOnly();
  }

  bool mipMapSupport() const override {
    return true;
  }

  std::shared_ptr<Texture> makeTexture(Context* context) const override;

  std::shared_ptr<Texture> makeMipMappedTexture(Context* context) const override;

 private:
  std::shared_ptr<Task> task = nullptr;
  std::shared_ptr<ImageBuffer> imageBuffer = nullptr;
  std::shared_ptr<ImageGenerator> generator = nullptr;

  explicit AsyncImageBuffer(std::shared_ptr<ImageGenerator> generator);

  void execute() override;
};
}  // namespace tgfx
