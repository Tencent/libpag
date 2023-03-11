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
  auto f = [&](std::unique_ptr<FragmentProcessor>& fp) {
    if (auto p = fp->instantiate()) {
      fp = std::move(p);
    }
  };
  std::for_each(_colors.begin(), _colors.end(), f);
  std::for_each(_masks.begin(), _masks.end(), f);
  onPrepare(gpu);
}

void DrawOp::execute(OpsRenderPass* opsRenderPass) {
  onExecute(opsRenderPass);
}

static std::shared_ptr<Texture> CreateDstTexture(OpsRenderPass* opsRenderPass, Rect dstRect,
                                                 Point* dstOffset) {
  if (opsRenderPass->context()->caps()->textureBarrierSupport &&
      opsRenderPass->renderTargetTexture()) {
    *dstOffset = {0, 0};
    return opsRenderPass->renderTargetTexture();
  }
  auto bounds =
      Rect::MakeWH(opsRenderPass->renderTarget()->width(), opsRenderPass->renderTarget()->height());
  if (opsRenderPass->renderTarget()->origin() == ImageOrigin::BottomLeft) {
    auto height = dstRect.height();
    dstRect.top = static_cast<float>(opsRenderPass->renderTarget()->height()) - dstRect.bottom;
    dstRect.bottom = dstRect.top + height;
  }
  if (!dstRect.intersect(bounds)) {
    return nullptr;
  }
  dstRect.roundOut();
  *dstOffset = {dstRect.x(), dstRect.y()};
  auto dstTexture = Texture::MakeRGBA(opsRenderPass->context(), static_cast<int>(dstRect.width()),
                                      static_cast<int>(dstRect.height()),
                                      opsRenderPass->renderTarget()->origin());
  if (dstTexture == nullptr) {
    LOGE("Failed to create dst texture(%f*%f).", dstRect.width(), dstRect.height());
    return nullptr;
  }
  opsRenderPass->context()->gpu()->copyRenderTargetToTexture(
      opsRenderPass->renderTarget().get(), dstTexture.get(), dstRect, Point::Zero());
  return dstTexture;
}

ProgramInfo DrawOp::createProgram(OpsRenderPass* opsRenderPass,
                                  std::unique_ptr<GeometryProcessor> gp) {
  auto numColorProcessors = _colors.size();
  std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors = {};
  fragmentProcessors.resize(numColorProcessors + _masks.size());
  std::move(_colors.begin(), _colors.end(), fragmentProcessors.begin());
  std::move(_masks.begin(), _masks.end(),
            fragmentProcessors.begin() + static_cast<int>(numColorProcessors));
  std::shared_ptr<Texture> dstTexture;
  Point dstTextureOffset = Point::Zero();
  ProgramInfo info;
  info.blendFactors = _blendFactors;
  if (_requiresDstTexture) {
    dstTexture = CreateDstTexture(opsRenderPass, bounds(), &dstTextureOffset);
  }
  const auto& swizzle = opsRenderPass->renderTarget()->writeSwizzle();
  info.pipeline =
      std::make_unique<Pipeline>(std::move(fragmentProcessors), numColorProcessors,
                                 std::move(_xferProcessor), dstTexture, dstTextureOffset, &swizzle);
  info.pipeline->setRequiresBarrier(dstTexture != nullptr &&
                                    dstTexture == opsRenderPass->renderTargetTexture());
  info.geometryProcessor = std::move(gp);
  return info;
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
         _blendFactors == that->_blendFactors && _requiresDstTexture == that->_requiresDstTexture &&
         CompareFragments(_colors, that->_colors) && CompareFragments(_masks, that->_masks) &&
         _xferProcessor == that->_xferProcessor;
}

}  // namespace tgfx
