/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/graphics/Canvas.h"
#include "tgfx/gpu/RuntimeEffect.h"

namespace pag {
class RenderCache;

class LayerStyleFilter {
 public:
  virtual ~LayerStyleFilter() = default;

  static std::unique_ptr<LayerStyleFilter> Make(LayerStyle* layerStyle, RenderCache* cache);

  virtual void update(Frame layerFrame, const tgfx::Point& filterScale,
                      const tgfx::Point& sourceScale) = 0;

  virtual bool draw(Canvas* canvas, std::shared_ptr<tgfx::Image> image) = 0;

 protected:
  RenderCache* cache = nullptr;
};

}  // namespace pag
