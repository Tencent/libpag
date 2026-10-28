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
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/nodes/LayerFilter.h"

namespace pagx {

/**
 * A brightness/contrast filter (spec 7.5): offsets brightness and scales contrast, migrated
 * from the legacy BrightnessContrast effect. The legacy useOldVersion flag changes the AE
 * authoring semantics but its renderer ignores it, so the node keeps it for data fidelity
 * only.
 */
class BrightnessContrastFilter : public LayerFilter {
 public:
  /**
   * The brightness offset. The default value is 0.
   */
  float brightness = 0.0f;

  /**
   * The contrast scale. The default value is 0.
   */
  float contrast = 0.0f;

  /**
   * Whether the legacy AE "old version" interpretation applies (data-only; the legacy renderer
   * ignores it). The default value is false.
   */
  bool useOldVersion = false;

  NodeType nodeType() const override {
    return NodeType::BrightnessContrastFilter;
  }

 private:
  BrightnessContrastFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
