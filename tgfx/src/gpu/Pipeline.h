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
#include "gpu/ProgramInfo.h"
#include "gpu/processors/EmptyXferProcessor.h"
#include "gpu/processors/FragmentProcessor.h"
#include "gpu/processors/GeometryProcessor.h"

namespace tgfx {
struct DstTextureInfo {
  bool requiresBarrier = false;
  Point offset = Point::Zero();
  std::shared_ptr<Texture> texture = nullptr;
};

/**
 * Pipeline is a ProgramInfo that uses a list of Processors to assemble a shader program and set API
 * state for a draw.
 */
class Pipeline : public ProgramInfo {
 public:
  Pipeline(std::unique_ptr<GeometryProcessor> geometryProcessor,
           std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors,
           size_t numColorProcessors, BlendMode blendMode, const DstTextureInfo& dstTextureInfo,
           const Swizzle* outputSwizzle);

  size_t numColorFragmentProcessors() const {
    return numColorProcessors;
  }

  size_t numFragmentProcessors() const {
    return fragmentProcessors.size();
  }

  const GeometryProcessor* getGeometryProcessor() const {
    return geometryProcessor.get();
  }

  const FragmentProcessor* getFragmentProcessor(size_t idx) const {
    return fragmentProcessors[idx].get();
  }

  const XferProcessor* getXferProcessor() const;

  const Swizzle* outputSwizzle() const {
    return _outputSwizzle;
  }

  const Texture* dstTexture() const {
    return dstTextureInfo.texture.get();
  }

  const Point& dstTextureOffset() const {
    return dstTextureInfo.offset;
  }

  bool requiresBarrier() const override {
    return dstTextureInfo.requiresBarrier;
  }

  const BlendInfo* blendInfo() const override {
    return xferProcessor == nullptr ? &_blendInfo : nullptr;
  }

  void getUniforms(UniformBuffer* uniformBuffer) const override;

  std::vector<SamplerInfo> getSamplers() const override;

  void computeUniqueKey(Context* context, BytesKey* uniqueKey) const override;

  std::unique_ptr<Program> createProgram(Context* context) const override;

 private:
  std::unique_ptr<GeometryProcessor> geometryProcessor;
  std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors;
  // This value is also the index in fragmentProcessors where coverage processors begin.
  size_t numColorProcessors = 0;
  std::unique_ptr<XferProcessor> xferProcessor;
  BlendInfo _blendInfo = {};
  DstTextureInfo dstTextureInfo = {};
  const Swizzle* _outputSwizzle = nullptr;
};
}  // namespace tgfx
