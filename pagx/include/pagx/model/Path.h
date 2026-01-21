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

#pragma once

#include "pagx/model/PathData.h"
#include "pagx/model/Element.h"

namespace pagx {

/**
 * Path represents a freeform shape defined by a PathData containing vertices, in-tangents, and
 * out-tangents.
 */
class Path : public Element {
 public:
  /**
   * The path data containing vertices and control points.
   */
  PathData data = {};

  /**
   * Whether the path direction is reversed. The default value is false.
   */
  bool reversed = false;

  ElementType type() const override {
    return ElementType::Path;
  }
};

}  // namespace pagx
