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

#include "StrokePathShape.h"
#include "core/utils/Log.h"
#include "tgfx/core/PathEffect.h"

namespace tgfx {
StrokePathShape::StrokePathShape(const Path& path, const Stroke& stroke, float resolutionScale)
    : ComplexShape(resolutionScale), path(path), stroke(stroke) {
  bounds = path.getBounds();
  bounds.makeOutset(stroke.width, stroke.width);
  bounds.scale(resolutionScale, resolutionScale);
}

Path StrokePathShape::getFinalPath() const {
  auto strokePath = path;
  auto effect = PathEffect::MakeStroke(stroke);
  DEBUG_ASSERT(effect != nullptr);
  effect->applyTo(&strokePath);
  strokePath.transform(Matrix::MakeScale(resolutionScale()));
  return strokePath;
}
}  // namespace tgfx
