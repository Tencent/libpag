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

#include "DrawOp.h"
#include "gpu/Gpu.h"
#include "utils/Log.h"

namespace tgfx {
void DrawOp::visitProxies(const std::function<void(TextureProxy*)>& func) const {
  auto f = [&](const std::unique_ptr<FragmentProcessor>& fp) { fp->visitProxies(func); };
  std::for_each(_colors.begin(), _colors.end(), f);
  std::for_each(_masks.begin(), _masks.end(), f);
}

void DrawOp::prepare(Gpu* gpu) {
  onPrepare(gpu);
}

void DrawOp::execute(RenderPass* renderPass) {
  onExecute(renderPass);
}

static DstTextureInfo CreateDstTextureInfo(RenderPass* renderPass, Rect dstRect) {
  DstTextureInfo dstTextureInfo = {};
  if (renderPass->context()->caps()->textureBarrierSupport && renderPass->renderTargetTexture()) {
    dstTextureInfo.texture = renderPass->renderTargetTexture();
    dstTextureInfo.requiresBarrier = true;
    return dstTextureInfo;
  }
  auto bounds =
      Rect::MakeWH(renderPass->renderTarget()->width(), renderPass->renderTarget()->height());
  if (renderPass->renderTarget()->origin() == ImageOrigin::BottomLeft) {
    auto height = dstRect.height();
    dstRect.top = static_cast<float>(renderPass->renderTarget()->height()) - dstRect.bottom;
    dstRect.bottom = dstRect.top + height;
  }
  if (!dstRect.intersect(bounds)) {
    return {};
  }
  dstRect.roundOut();
  dstTextureInfo.offset = {dstRect.x(), dstRect.y()};
  auto dstTexture =
      Texture::MakeRGBA(renderPass->context(), static_cast<int>(dstRect.width()),
                        static_cast<int>(dstRect.height()), renderPass->renderTarget()->origin());
  if (dstTexture == nullptr) {
    LOGE("Failed to create dst texture(%f*%f).", dstRect.width(), dstRect.height());
    return {};
  }
  dstTextureInfo.texture = dstTexture;
  renderPass->context()->gpu()->copyRenderTargetToTexture(renderPass->renderTarget().get(),
                                                          dstTexture.get(), dstRect, Point::Zero());
  return dstTextureInfo;
}

std::unique_ptr<Pipeline> DrawOp::createPipeline(RenderPass* renderPass,
                                                 std::unique_ptr<GeometryProcessor> gp) {
  auto numColorProcessors = _colors.size();
  std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors = {};
  fragmentProcessors.resize(numColorProcessors + _masks.size());
  std::move(_colors.begin(), _colors.end(), fragmentProcessors.begin());
  std::move(_masks.begin(), _masks.end(),
            fragmentProcessors.begin() + static_cast<int>(numColorProcessors));
  DstTextureInfo dstTextureInfo = {};
  auto caps = renderPass->context()->caps();
  if (!BlendModeAsCoeff(blendMode) && !caps->frameBufferFetchSupport) {
    dstTextureInfo = CreateDstTextureInfo(renderPass, bounds());
  }
  const auto& swizzle = renderPass->renderTarget()->writeSwizzle();
  return std::make_unique<Pipeline>(std::move(gp), std::move(fragmentProcessors),
                                    numColorProcessors, blendMode, dstTextureInfo, &swizzle);
}

static bool CompareFragments(const std::vector<std::unique_ptr<FragmentProcessor>>& frags1,
                             const std::vector<std::unique_ptr<FragmentProcessor>>& frags2) {
  if (frags1.size() != frags2.size()) {
    return false;
  }
  for (size_t i = 0; i < frags1.size(); i++) {
    if (!frags1[i]->isEqual(*frags2[i])) {
      return false;
    }
  }
  return true;
}

bool DrawOp::onCombineIfPossible(Op* op) {
  auto* that = static_cast<DrawOp*>(op);
  return aa == that->aa && _scissorRect == that->_scissorRect &&
         CompareFragments(_colors, that->_colors) && CompareFragments(_masks, that->_masks) &&
         blendMode == that->blendMode;
}

}  // namespace tgfx
