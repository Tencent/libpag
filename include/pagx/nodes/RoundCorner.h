/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "pagx/nodes/Element.h"

namespace pagx {

/**
 * RoundCorner is a path modifier that rounds the corners of shapes by adding smooth curves at
 * sharp vertices.
 */
class RoundCorner : public Element {
 public:
  /**
   * The radius of the rounded corners in pixels. The default value is 10.
   */
  float radius = 10;

  NodeType nodeType() const override {
    return NodeType::RoundCorner;
  }

 private:
  RoundCorner() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
