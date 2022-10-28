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

#include "GpuBuffer.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/Texture.h"

namespace tgfx {
class GradientCache;

class ResourceProvider {
 public:
  explicit ResourceProvider(Context* context) : context(context) {
  }

  ~ResourceProvider();

  std::shared_ptr<Texture> getGradient(const Color* colors, const float* positions, int count);

  std::shared_ptr<GpuBuffer> nonAAQuadIndexBuffer();

  static uint16_t MaxNumNonAAQuads();

  static uint16_t NumIndicesPerNonAAQuad();

  std::shared_ptr<GpuBuffer> aaQuadIndexBuffer();

  static uint16_t MaxNumAAQuads();

  static uint16_t NumIndicesPerAAQuad();

  void releaseAll();

 private:
  std::shared_ptr<GpuBuffer> createNonAAQuadIndexBuffer();

  std::shared_ptr<GpuBuffer> createAAQuadIndexBuffer();

  Context* context = nullptr;
  GradientCache* _gradientCache = nullptr;
  std::shared_ptr<GpuBuffer> _aaQuadIndexBuffer;
  std::shared_ptr<GpuBuffer> _nonAAQuadIndexBuffer;
};
}  // namespace tgfx
