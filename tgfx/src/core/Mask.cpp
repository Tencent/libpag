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

#include "tgfx/core/Mask.h"
#include "tgfx/core/PathEffect.h"

namespace tgfx {
bool Mask::CanUseAsMask(const TextBlob* textBlob) {
  if (textBlob == nullptr) {
    return false;
  }
  auto typeface = textBlob->font.getTypeface();
  if (typeface->hasColor()) {
    return false;
  }
  return true;
}

void Mask::strokePath(const Path& path, const Stroke& stroke) {
  std::unique_ptr<PathEffect> pathEffect = PathEffect::MakeStroke(stroke);
  if (pathEffect == nullptr) {
    return;
  }
  auto newPath = path;
  pathEffect->applyTo(&newPath);
  fillPath(newPath);
}

bool Mask::fillText(const TextBlob* textBlob) {
  if (!CanUseAsMask(textBlob)) {
    return false;
  }
  Path path = {};
  if (!textBlob->getPath(&path)) {
    return false;
  }
  fillPath(path);
  return true;
}

bool Mask::strokeText(const TextBlob* textBlob, const Stroke& stroke) {
  if (!CanUseAsMask(textBlob)) {
    return false;
  }
  Path path = {};
  if (!textBlob->getPath(&path, &stroke)) {
    return false;
  }
  fillPath(path);
  return true;
}
}  // namespace tgfx
