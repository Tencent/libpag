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
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Node.h"

namespace pagx {

// ============================================================================
// Layer classification
// ============================================================================

/**
 * Returns true if the Layer uses any feature that Group does not support at all (e.g. blendMode,
 * styles, filters, mask, 3D transforms, container layout, composition, alpha with offscreen
 * semantics). Does NOT check contents or children (callers handle those based on context), nor
 * attributes whose values can be mechanically transferred to a Group (2D matrix, x/y position,
 * width/height, padding, constraint positioning). When adding a new Layer-only attribute, add a
 * corresponding check here; otherwise the attribute will be silently ignored by the "can this
 * Layer be downgraded to a Group?" / "is this a pure shell Layer?" logic shared by the cli
 * verify / resolve commands and by PAGXOptimizer.
 */
bool HasLayerOnlyFeatures(const Layer* layer);

/**
 * Returns true if the Layer is a plain shell — all attributes are at their default values. Does
 * NOT check contents (the payload to retain). Stricter than HasLayerOnlyFeatures: also requires
 * attributes transferable to Group (x/y position, 2D matrix, size, padding, constraints) to be
 * at their defaults.
 */
bool IsLayerShell(const Layer* layer);

// ============================================================================
// Element / painter classification
// ============================================================================

/**
 * Returns true if `type` denotes a painter (Fill or Stroke). Painter elements consume geometry
 * that precedes them inside the enclosing Group; this predicate is used pervasively to detect
 * painter-isolation boundaries and paint-order leaks.
 */
inline bool IsPainter(NodeType type) {
  return type == NodeType::Fill || type == NodeType::Stroke;
}

/**
 * Returns true if `elements` contains at least one painter (Fill or Stroke).
 */
bool ContainsPainter(const std::vector<Element*>& elements);

}  // namespace pagx
