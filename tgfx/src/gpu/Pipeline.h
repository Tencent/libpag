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

#include "Swizzle.h"
#include "gpu/Blend.h"
#include "gpu/processors/EmptyXferProcessor.h"
#include "gpu/processors/FragmentProcessor.h"
#include "gpu/processors/GeometryProcessor.h"

namespace tgfx {
/**
 * This immutable object contains information needed to build a shader program and set API state for
 * a draw.
 */
class Pipeline {
 public:
  Pipeline(std::unique_ptr<GeometryProcessor> geometryProcessor,
           std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors,
           size_t numColorProcessors, BlendMode blendMode, std::shared_ptr<Texture> dstTexture,
           Point dstTextureOffset, const Swizzle* outputSwizzle);

  void computeKey(Context* context, BytesKey* bytesKey) const;

  size_t numColorFragmentProcessors() const {
    return numColorProcessors;
  }

  size_t numFragmentProcessors() const {
    return fragmentProcessors.size();
  }

  const GeometryProcessor* getGeometryProcessor() const {
    return geometryProcessor.get();
  }

  const XferProcessor* getXferProcessor() const;

  const Texture* getDstTexture(Point* offset = nullptr) const;

  const FragmentProcessor* getFragmentProcessor(size_t idx) const {
    return fragmentProcessors[idx].get();
  }

  void setRequiresBarrier(bool requiresBarrier) {
    _requiresBarrier = requiresBarrier;
  }

  bool requiresBarrier() const {
    return _requiresBarrier;
  }

  const Swizzle* outputSwizzle() const {
    return _outputSwizzle;
  }

  const BlendInfo* blendInfo() const {
    return xferProcessor == nullptr ? &_blendInfo : nullptr;
  }

 private:
  std::unique_ptr<GeometryProcessor> geometryProcessor;
  std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors;
  // This value is also the index in fragmentProcessors where coverage processors begin.
  size_t numColorProcessors = 0;
  std::unique_ptr<XferProcessor> xferProcessor;
  BlendInfo _blendInfo = {};
  std::shared_ptr<Texture> dstTexture;
  bool _requiresBarrier = false;
  Point dstTextureOffset = Point::Zero();
  const Swizzle* _outputSwizzle = nullptr;
};
}  // namespace tgfx
