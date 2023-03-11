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

#pragma once

#include "gpu/Texture.h"

namespace tgfx {
/**
 * Texture created externally to TGFX
 */
class ExternalTexture : public Texture {
 public:
  static std::shared_ptr<Texture> MakeFrom(Context* context, const BackendTexture& backendTexture,
                                           ImageOrigin origin, bool adopted);

  size_t memoryUsage() const override;

  const TextureSampler* getSampler() const override {
    return sampler.get();
  }

 private:
  std::unique_ptr<TextureSampler> sampler = {};
  bool adopted = false;

  ExternalTexture(std::unique_ptr<TextureSampler> sampler, int width, int height,
                  ImageOrigin origin, bool adopted);

  void onReleaseGPU() override;
};
}  // namespace tgfx
