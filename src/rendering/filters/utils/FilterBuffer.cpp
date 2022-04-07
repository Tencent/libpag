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

#include "FilterBuffer.h"
#include "FilterHelper.h"

namespace pag {
std::shared_ptr<FilterBuffer> FilterBuffer::Make(tgfx::Context* context, int width, int height,
                                                 bool usesMSAA) {
  auto sampleCount = usesMSAA ? 4 : 1;
  auto surface = tgfx::Surface::Make(context, width, height, false, sampleCount);
  if (surface == nullptr) {
    return nullptr;
  }
  auto buffer = new FilterBuffer();
  buffer->surface = surface;
  return std::shared_ptr<FilterBuffer>(buffer);
}

tgfx::GLFrameBuffer FilterBuffer::getFramebuffer() const {
  auto renderTarget = std::static_pointer_cast<tgfx::GLRenderTarget>(surface->getRenderTarget());
  return renderTarget->glFrameBuffer();
}

tgfx::GLSampler FilterBuffer::getTexture() const {
  auto texture = std::static_pointer_cast<tgfx::GLTexture>(surface->getTexture());
  return texture->glSampler();
}

void FilterBuffer::clearColor() const {
  surface->getCanvas()->clear();
}

std::unique_ptr<FilterSource> FilterBuffer::toFilterSource(const tgfx::Point& scale) const {
  auto filterSource = new FilterSource();
  filterSource->sampler = getTexture();
  filterSource->width = surface->width();
  filterSource->height = surface->height();
  filterSource->scale = scale;
  filterSource->textureMatrix = ToGLTextureMatrix(tgfx::Matrix::I(), surface->width(),
                                                  surface->height(), tgfx::ImageOrigin::TopLeft);
  return std::unique_ptr<FilterSource>(filterSource);
}

std::unique_ptr<FilterTarget> FilterBuffer::toFilterTarget(
    const tgfx::Matrix& drawingMatrix) const {
  auto filterTarget = new FilterTarget();
  filterTarget->frameBuffer = getFramebuffer();
  filterTarget->width = surface->width();
  filterTarget->height = surface->height();
  filterTarget->vertexMatrix = ToGLVertexMatrix(drawingMatrix, surface->width(), surface->height(),
                                                tgfx::ImageOrigin::TopLeft);
  return std::unique_ptr<FilterTarget>(filterTarget);
}
}  // namespace pag
