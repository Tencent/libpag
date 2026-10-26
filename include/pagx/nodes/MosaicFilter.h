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

namespace pagx {

/**
 * A mosaic filter (spec 7.5): quantizes the layer into rectangular blocks, migrated from the
 * legacy Mosaic effect.
 */
class MosaicFilter : public LayerFilter {
 public:
  /**
   * The number of horizontal blocks. The default value is 1.
   */
  float horizontalBlocks = 1.0f;

  /**
   * The number of vertical blocks. The default value is 1.
   */
  float verticalBlocks = 1.0f;

  /**
   * Whether block colors snap to a single representative color instead of averaging.
   * The default value is false.
   */
  bool sharpColors = false;

  NodeType nodeType() const override {
    return NodeType::MosaicFilter;
  }

 private:
  MosaicFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
