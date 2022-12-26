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

#include "tgfx/gpu/opengl/GLRenderTarget.h"
#include "tgfx/src/platform/android/HardwareBuffer.h"

namespace pag {
class HardwareBufferRenderTarget : public tgfx::GLRenderTarget {
 public:
  static std::shared_ptr<HardwareBufferRenderTarget> MakeFrom(
      tgfx::Context* context, std::shared_ptr<tgfx::HardwareBuffer> hardwareBuffer,
      std::shared_ptr<tgfx::Texture> texture);

  bool readPixels(const tgfx::ImageInfo& dstInfo, void* dstPixels, int srcX = 0,
                  int srcY = 0) const override;

  void onReleaseGPU() override;

  const tgfx::Swizzle& writeSwizzle() const override;

  tgfx::GLFrameBuffer glFrameBuffer() const override;

  void resolve() const override;

 private:
  HardwareBufferRenderTarget(std::shared_ptr<tgfx::HardwareBuffer> hardwareBuffer,
                             std::shared_ptr<tgfx::GLRenderTarget> glRenderTarget);

  std::shared_ptr<tgfx::HardwareBuffer> hardwareBuffer;

  std::shared_ptr<tgfx::GLRenderTarget> glRenderTarget;
};
}  // namespace pag
