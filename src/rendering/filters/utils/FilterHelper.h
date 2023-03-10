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

#include "base/utils/Log.h"
#include "base/utils/TGFXCast.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/filters/Filter.h"
#include "tgfx/gpu/opengl/GLFunctions.h"

namespace pag {
std::array<float, 9> ToGLMatrix(const tgfx::Matrix& matrix);

std::array<float, 9> ToGLVertexMatrix(const tgfx::Matrix& matrix, int width, int height,
                                      tgfx::SurfaceOrigin origin);

std::array<float, 9> ToGLTextureMatrix(const tgfx::Matrix& matrix, int width, int height,
                                       tgfx::SurfaceOrigin origin);

tgfx::Matrix ToMatrix(const FilterTarget* target, bool flipY = false);

std::unique_ptr<FilterSource> ToFilterSource(const tgfx::BackendTexture& texture,
                                             tgfx::SurfaceOrigin origin, const tgfx::Point& scale);

std::unique_ptr<FilterTarget> ToFilterTarget(tgfx::Surface* surface,
                                             const tgfx::Matrix& drawingMatrix);

tgfx::Point ToGLTexturePoint(const FilterSource* source, const tgfx::Point& texturePoint);

tgfx::Point ToGLVertexPoint(const FilterTarget* target, const FilterSource* source,
                            const tgfx::Rect& contentBounds, const tgfx::Point& contentPoint);

unsigned CreateGLProgram(tgfx::Context* context, const std::string& vertex,
                         const std::string& fragment);

void ActiveGLTexture(tgfx::Context* context, int unitIndex, const tgfx::GLTextureInfo* sampler);

bool CheckGLError(tgfx::Context* context);

}  // namespace pag
