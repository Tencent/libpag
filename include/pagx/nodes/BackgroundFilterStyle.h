/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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
#include <vector>
#include "pagx/nodes/LayerFilter.h"
#include "pagx/nodes/LayerStyle.h"
#include "pagx/nodes/PathData.h"

namespace pagx {
/** Applies an ordered filter chain to the backdrop, clipped to the layer contour.
 * The layer's own paint and children are composited afterwards and are not filtered.
 */
class BackgroundFilterStyle : public LayerStyle {
 public:
  std::vector<LayerFilter*> filters = {};
  // Optional clip in owner-layer coordinates; absent means the layer contour.
  PathData* clipPath = nullptr;
  NodeType nodeType() const override {
    return NodeType::BackgroundFilterStyle;
  }

 private:
  BackgroundFilterStyle() = default;
  friend class PAGXDocument;
};
}  // namespace pagx
