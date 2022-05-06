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

#include "ColorReplacement.h"
#include <base/utils/TGFXCast.h>
#include "rendering/editing/ReplacementHolder.h"
#include "rendering/graphics/Shape.h"

namespace pag {
ColorReplacement::ColorReplacement(PAGSolidLayer* pagLayer, ReplacementHolder<Color>* colorHolder,
                                   int editableIndex)
    : colorHolder(colorHolder), editableIndex(editableIndex), pagLayer(pagLayer) {
}

ColorReplacement::~ColorReplacement() {
  delete colorContentCache;
}

Content* ColorReplacement::getContent(Frame) {
  if (colorContentCache == nullptr) {
    auto color = colorHolder->getReplacement(editableIndex);
    tgfx::Path path = {};
    auto solidLayer = static_cast<SolidLayer*>(pagLayer->layer);
    path.addRect(0, 0, solidLayer->width, solidLayer->height);
    auto solid = Shape::MakeFrom(path, ToTGFX(*color));
    colorContentCache = new GraphicContent(solid);
  }
  return colorContentCache;
}

Color ColorReplacement::solidColor() {
  return *colorHolder->getReplacement(editableIndex);
}

void ColorReplacement::clearCache() {
  delete colorContentCache;
  colorContentCache = nullptr;
}
}  // namespace pag
