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

#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/filters/Filter.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/opengl/GLRenderTarget.h"

namespace pag {
class FilterBuffer {
 public:
  static std::shared_ptr<FilterBuffer> Make(tgfx::Context* context, int width, int height,
                                            bool usesMSAA = false);

  void clearColor() const;

  std::unique_ptr<FilterSource> toFilterSource(const tgfx::Point& scale) const;

  std::unique_ptr<FilterTarget> toFilterTarget(const tgfx::Matrix& drawingMatrix) const;

  int width() const {
    return surface->width();
  }

  int height() const {
    return surface->height();
  }

  bool usesMSAA() const {
    return surface->getRenderTarget()->sampleCount() > 1;
  }

  tgfx::GLFrameBuffer getFramebuffer() const;

  tgfx::GLSampler getTexture() const;

 private:
  std::shared_ptr<tgfx::Surface> surface = nullptr;

  FilterBuffer() = default;
};
}  // namespace pag
