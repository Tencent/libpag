/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "GraphicContent.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/graphics/Recorder.h"

namespace pag {
GraphicContent::GraphicContent(std::shared_ptr<Graphic> graphic) : graphic(std::move(graphic)) {
}

void GraphicContent::measureBounds(tgfx::Rect* bounds) {
  if (graphic) {
    graphic->measureBounds(bounds);
  } else {
    bounds->setEmpty();
  }
}

void GraphicContent::draw(Recorder* recorder) {
  recorder->drawGraphic(graphic);
}
}  // namespace pag
