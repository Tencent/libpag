/////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <vector>
#include "pagx/types/Point.h"
#include "pagx/types/Rect.h"

namespace pagx {

/** A captured horizontal line fragment. Ranges count Unicode scalars in the logical Text,
 * not UTF-16 units or glyphs. textIndex addresses Texts in TextBox traversal order. Positions
 * are cluster anchors relative to the alphabetic origin; bounds use TextBox content coordinates.
 * Gaps between ranges may contain only non-painting ASCII whitespace (e.g. a soft-wrap space).
 */
struct TextLayoutFragment {
  uint32_t textIndex = 0;
  uint32_t start = 0;
  uint32_t length = 0;
  Point position = {};
  Rect bounds = {};
  std::vector<float> inlinePositions = {};
};

}  // namespace pagx
