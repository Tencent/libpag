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

#include "EmptyXferProcessor.h"
#include "Swizzle.h"
#include "gpu/FragmentProcessor.h"

namespace tgfx {
class Pipeline {
 public:
  Pipeline(std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors,
           size_t numColorProcessors, std::unique_ptr<XferProcessor> xferProcessor,
           std::shared_ptr<Texture> dstTexture, Point dstTextureOffset,
           const Swizzle* outputSwizzle);

  void computeKey(Context* context, BytesKey* bytesKey) const;

  size_t numColorFragmentProcessors() const {
    return numColorProcessors;
  }

  size_t numFragmentProcessors() const {
    return fragmentProcessors.size();
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

 private:
  std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors;
  // This value is also the index in fragmentProcessors where coverage processors begin.
  size_t numColorProcessors = 0;
  std::unique_ptr<XferProcessor> xferProcessor;
  std::shared_ptr<Texture> dstTexture;
  bool _requiresBarrier = false;
  Point dstTextureOffset = Point::Zero();
  const Swizzle* _outputSwizzle = nullptr;
};
}  // namespace tgfx
