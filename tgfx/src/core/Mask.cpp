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
  if (textBlob == nullptr || textBlob->hasColor()) {
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
  if (textBlob == nullptr || textBlob->hasColor()) {
    return false;
  }
  Path path = {};
  if (!textBlob->getPath(&path, &stroke)) {
    return false;
  }
  fillPath(path);
  return true;
}

std::shared_ptr<tgfx::Image> Mask::makeImage(Context* context) {
  auto texture = updateTexture(context);
  return Image::MakeFrom(texture);
}
}  // namespace tgfx
