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

#include "gpu/Texture.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/filters/Filter.h"

namespace pag {
Matrix ToMatrix(const FilterTarget* target, bool flipY = false);

std::unique_ptr<FilterSource> ToFilterSource(const Texture* texture, const Point& scale);

std::unique_ptr<FilterTarget> ToFilterTarget(const Surface* surface, const Matrix& drawingMatrix);

Point ToGLTexturePoint(const FilterSource* source, const Point& texturePoint);

Point ToGLVertexPoint(const FilterTarget* target, const FilterSource* source,
                      const Rect& contentBounds, const Point& contentPoint);

void PreConcatMatrix(FilterTarget* target, const Matrix& matrix);
}  // namespace pag
