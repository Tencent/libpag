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

#include "Pipeline.h"
#include "gpu/ProgramBuilder.h"
#include "gpu/StagedUniformBuffer.h"
#include "gpu/TextureSampler.h"
#include "gpu/processors/PorterDuffXferProcessor.h"

namespace tgfx {
Pipeline::Pipeline(std::unique_ptr<GeometryProcessor> geometryProcessor,
                   std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors,
                   size_t numColorProcessors, BlendMode blendMode,
                   const DstTextureInfo& dstTextureInfo, const Swizzle* outputSwizzle)
    : geometryProcessor(std::move(geometryProcessor)),
      fragmentProcessors(std::move(fragmentProcessors)),
      numColorProcessors(numColorProcessors),
      dstTextureInfo(dstTextureInfo),
      _outputSwizzle(outputSwizzle) {
  if (!BlendModeAsCoeff(blendMode, &_blendInfo)) {
    xferProcessor = PorterDuffXferProcessor::Make(blendMode);
  }
}

const XferProcessor* Pipeline::getXferProcessor() const {
  if (xferProcessor == nullptr) {
    return EmptyXferProcessor::GetInstance();
  }
  return xferProcessor.get();
}

void Pipeline::getUniforms(UniformBuffer* uniformBuffer) const {
  auto buffer = static_cast<StagedUniformBuffer*>(uniformBuffer);
  buffer->advanceStage();
  FragmentProcessor::CoordTransformIter coordTransformIter(this);
  geometryProcessor->setData(buffer, &coordTransformIter);
  for (auto& fragmentProcessor : fragmentProcessors) {
    buffer->advanceStage();
    FragmentProcessor::Iter iter(fragmentProcessor.get());
    const FragmentProcessor* fp = iter.next();
    while (fp) {
      fp->setData(buffer);
      fp = iter.next();
    }
  }
  if (dstTextureInfo.texture != nullptr) {
    buffer->advanceStage();
    xferProcessor->setData(buffer, dstTextureInfo.texture.get(), dstTextureInfo.offset);
  }
  buffer->resetStage();
}

std::vector<SamplerInfo> Pipeline::getSamplers() const {
  std::vector<SamplerInfo> samplers = {};
  FragmentProcessor::Iter iter(this);
  const FragmentProcessor* fp = iter.next();
  while (fp) {
    for (size_t i = 0; i < fp->numTextureSamplers(); ++i) {
      SamplerInfo sampler = {fp->textureSampler(i), fp->samplerState(i)};
      samplers.push_back(sampler);
    }
    fp = iter.next();
  }
  if (dstTextureInfo.texture != nullptr) {
    SamplerInfo sampler = {dstTextureInfo.texture->getSampler(), {}};
    samplers.push_back(sampler);
  }
  return samplers;
}

void Pipeline::computeUniqueKey(Context* context, BytesKey* bytesKey) const {
  geometryProcessor->computeProcessorKey(context, bytesKey);
  if (dstTextureInfo.texture != nullptr) {
    dstTextureInfo.texture->getSampler()->computeKey(context, bytesKey);
  }
  for (const auto& processor : fragmentProcessors) {
    processor->computeProcessorKey(context, bytesKey);
  }
  getXferProcessor()->computeProcessorKey(context, bytesKey);
  bytesKey->write(static_cast<uint32_t>(_outputSwizzle->asKey()));
}

std::unique_ptr<Program> Pipeline::createProgram(Context* context) const {
  return ProgramBuilder::CreateProgram(context, this);
}
}  // namespace tgfx
