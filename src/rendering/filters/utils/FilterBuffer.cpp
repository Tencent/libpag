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
#include "gpu/opengl/GLUtil.h"

namespace pag {
std::shared_ptr<FilterBuffer> FilterBuffer::Make(Context* context, int width, int height,
        bool usesMSAA) {
    auto texture = GLTexture::MakeRGBA(context, width, height);
    auto gl = GLContext::Unwrap(context);
    auto sampleCount = usesMSAA ? gl->caps->getSampleCount(4, PixelConfig::RGBA_8888) : 1;
    auto renderTarget = GLRenderTarget::MakeFrom(context, texture.get(), sampleCount);
    if (renderTarget == nullptr) {
        return nullptr;
    }
    auto buffer = new FilterBuffer();
    buffer->texture = texture;
    buffer->renderTarget = renderTarget;
    return std::shared_ptr<FilterBuffer>(buffer);
}

void FilterBuffer::resolve(Context* context) {
    renderTarget->resolve(context);
}

void FilterBuffer::clearColor(const GLInterface* gl) const {
    renderTarget->clear(gl);
}

std::unique_ptr<FilterSource> FilterBuffer::toFilterSource(const Point& scale) const {
    auto filterSource = new FilterSource();
    filterSource->textureID = getTexture().id;
    filterSource->width = texture->width();
    filterSource->height = texture->height();
    filterSource->scale = scale;
    // TODO(domrjchen): 这里的 ImageOrigin 是错的
    filterSource->textureMatrix =
        ToGLTextureMatrix(Matrix::I(), texture->width(), texture->height(), ImageOrigin::BottomLeft);
    return std::unique_ptr<FilterSource>(filterSource);
}

std::unique_ptr<FilterTarget> FilterBuffer::toFilterTarget(const Matrix& drawingMatrix) const {
    auto filterTarget = new FilterTarget();
    filterTarget->frameBufferID = getFramebuffer().id;
    filterTarget->width = renderTarget->width();
    filterTarget->height = renderTarget->height();
    filterTarget->vertexMatrix = ToGLVertexMatrix(drawingMatrix, renderTarget->width(),
                                 renderTarget->height(), ImageOrigin::BottomLeft);
    return std::unique_ptr<FilterTarget>(filterTarget);
}
}  // namespace pag
