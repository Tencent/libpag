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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/nodes/LayerFilter.h"
#include "pagx/types/Point.h"

namespace pagx {

/**
 * A corner pin filter (spec 7.5): warps the layer with a projective transform driven by four
 * corner points, migrated from the legacy CornerPin effect.
 */
class CornerPinFilter : public LayerFilter {
 public:
  /**
   * The pinned upper-left corner in layer coordinates. The default value is (0, 0).
   */
  Point upperLeft = {};

  /**
   * The pinned upper-right corner in layer coordinates. The default value is (0, 0).
   */
  Point upperRight = {};

  /**
   * The pinned lower-left corner in layer coordinates. The default value is (0, 0).
   */
  Point lowerLeft = {};

  /**
   * The pinned lower-right corner in layer coordinates. The default value is (0, 0).
   */
  Point lowerRight = {};

  NodeType nodeType() const override {
    return NodeType::CornerPinFilter;
  }

 private:
  CornerPinFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
