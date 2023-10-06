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
#include "gpu/PorterDuffXferProcessor.h"
#include "gpu/TextureSampler.h"

namespace tgfx {
Pipeline::Pipeline(std::unique_ptr<GeometryProcessor> geometryProcessor,
                   std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors,
                   size_t numColorProcessors, BlendMode blendMode,
                   std::shared_ptr<Texture> dstTexture, Point dstTextureOffset,
                   const Swizzle* outputSwizzle)
    : geometryProcessor(std::move(geometryProcessor)),
      fragmentProcessors(std::move(fragmentProcessors)),
      numColorProcessors(numColorProcessors),
      dstTexture(std::move(dstTexture)),
      dstTextureOffset(dstTextureOffset),
      _outputSwizzle(outputSwizzle) {
  if (!BlendModeAsCoeff(blendMode, &_blendInfo)) {
    xferProcessor = PorterDuffXferProcessor::Make(blendMode);
  }
}

void Pipeline::computeKey(Context* context, BytesKey* bytesKey) const {
  geometryProcessor->computeProcessorKey(context, bytesKey);
  if (dstTexture != nullptr) {
    dstTexture->getSampler()->computeKey(context, bytesKey);
  }
  for (const auto& processor : fragmentProcessors) {
    processor->computeProcessorKey(context, bytesKey);
  }
  getXferProcessor()->computeProcessorKey(context, bytesKey);
  bytesKey->write(static_cast<uint32_t>(_outputSwizzle->asKey()));
}

const XferProcessor* Pipeline::getXferProcessor() const {
  if (xferProcessor == nullptr) {
    return EmptyXferProcessor::GetInstance();
  }
  return xferProcessor.get();
}

const Texture* Pipeline::getDstTexture(Point* offset) const {
  if (dstTexture == nullptr) {
    return nullptr;
  }
  if (offset) {
    *offset = dstTextureOffset;
  }
  return dstTexture.get();
}
}  // namespace tgfx
