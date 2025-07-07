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

#pragma once

#include "TrackMatteRenderer.h"
#include "pag/pag.h"

namespace pag {
class LayerRenderer {
 public:
  static void DrawLayer(Recorder* recorder, Layer* layer, Frame layerFrame,
                        std::shared_ptr<FilterModifier> filterModifier = nullptr,
                        TrackMatte* trackMatte = nullptr, Content* layerContent = nullptr,
                        Transform* extraTransform = nullptr);

  static void MeasureLayerBounds(tgfx::Rect* bounds, Layer* layer, Frame layerFrame,
                                 std::shared_ptr<FilterModifier> filterModifier = nullptr,
                                 tgfx::Rect* trackMatteBounds = nullptr, Content* content = nullptr,
                                 Transform* extraTransform = nullptr);
};

}  // namespace pag
