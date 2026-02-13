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

#include "pagx/nodes/Element.h"
#include "pagx/types/MergePathMode.h"

namespace pagx {

/**
 * MergePath is a path modifier that merges multiple paths using boolean operations. It can append,
 * add, subtract, intersect, or exclude paths from each other.
 */
class MergePath : public Element {
 public:
  /**
   * The merge mode that determines how paths are combined. The default value is Append.
   */
  MergePathMode mode = MergePathMode::Append;

  NodeType nodeType() const override {
    return NodeType::MergePath;
  }

 private:
  MergePath() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
