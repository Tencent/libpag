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

#include "base/utils/TGFXCast.h"
#include "gpu/Texture.h"
#include "gpu/opengl/GLUtil.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/filters/Filter.h"

namespace pag {
tgfx::Matrix ToMatrix(const FilterTarget* target, bool flipY = false);

std::unique_ptr<FilterSource> ToFilterSource(const tgfx::Texture* texture,
                                             const tgfx::Point& scale);

std::unique_ptr<FilterTarget> ToFilterTarget(const tgfx::Surface* surface,
                                             const tgfx::Matrix& drawingMatrix);

tgfx::Point ToGLTexturePoint(const FilterSource* source, const tgfx::Point& texturePoint);

tgfx::Point ToGLVertexPoint(const FilterTarget* target, const FilterSource* source,
                            const tgfx::Rect& contentBounds, const tgfx::Point& contentPoint);

void PreConcatMatrix(FilterTarget* target, const tgfx::Matrix& matrix);
}  // namespace pag
