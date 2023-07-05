/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/Mask.h"
#include "core/ImageStream.h"
#include "tgfx/core/PathEffect.h"

namespace tgfx {
void Mask::fillPath(const Path& path, const Stroke* stroke) {
  if (path.isEmpty()) {
    return;
  }
  auto effect = PathEffect::MakeStroke(stroke);
  if (effect != nullptr) {
    auto newPath = path;
    effect->applyTo(&newPath);
    onFillPath(newPath, matrix);
  } else {
    onFillPath(path, matrix);
  }
}

bool Mask::fillText(const TextBlob* textBlob, const Stroke* stroke) {
  if (textBlob == nullptr || textBlob->hasColor()) {
    return false;
  }
  if (onFillText(textBlob, stroke, matrix)) {
    return true;
  }
  Path path = {};
  if (textBlob->getPath(&path, stroke)) {
    onFillPath(path, matrix, true);
    return true;
  }
  return false;
}

bool Mask::onFillText(const TextBlob*, const Stroke*, const Matrix&) {
  return false;
}
}  // namespace tgfx
