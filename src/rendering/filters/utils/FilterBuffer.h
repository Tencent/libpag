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

#include "gpu/opengl/GLRenderTarget.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/filters/Filter.h"

namespace pag {
class FilterBuffer {
public:
    static std::shared_ptr<FilterBuffer> Make(Context* context, int width, int height,
            bool usesMSAA = false);

    void clearColor(const GLInterface* gl) const;

    std::unique_ptr<FilterSource> toFilterSource(const Point& scale) const;

    std::unique_ptr<FilterTarget> toFilterTarget(const Matrix& drawingMatrix) const;

    int width() const {
        return renderTarget->width();
    }

    int height() const {
        return renderTarget->height();
    }

    bool usesMSAA() const {
        return renderTarget->usesMSAA();
    }

    GLFrameBufferInfo getFramebuffer() const {
        return renderTarget->getGLInfo();
    }

    GLTextureInfo getTexture() const {
        return texture->getGLInfo();
    }

    void resolve(Context* context);

private:
    std::shared_ptr<GLRenderTarget> renderTarget = nullptr;
    std::shared_ptr<GLTexture> texture = nullptr;
    FilterBuffer() = default;
};
}  // namespace pag
