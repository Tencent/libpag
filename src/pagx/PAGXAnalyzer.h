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

#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"

namespace pagx {

enum class PathPrimitive { None, Rectangle, Ellipse };

struct PrimitiveInfo {
  float cx = 0;
  float cy = 0;
  float w = 0;
  float h = 0;
};

/**
 * Shared predicate functions used by both PAGXOptimizer and CommandVerify to analyze document
 * structure. Each function is a pure query with no side effects.
 */
class PAGXAnalyzer {
 public:
  static bool IsEmptyLayer(const Layer* layer, bool parentHasLayout);
  static bool IsEmptyGroup(const Group* group);
  static bool CanDowngradeLayerToGroup(const Layer* layer);
  static bool HasDefaultGroupTransform(const Group* group);
  static bool CanUnwrapFirstChildGroup(const Group* group);
  static PathPrimitive DetectPathPrimitive(const Path* path, PrimitiveInfo* infoOut);
};

}  // namespace pagx
