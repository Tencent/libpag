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

#include <memory>
#include <unordered_map>
#include <vector>
#include "tgfx/core/Point.h"
#include "tgfx/core/TextBlob.h"

namespace pagx {

class Text;

/**
 * Shaped text data containing the TextBlob and per-glyph anchor offsets.
 */
struct ShapedText {
  /**
   * The shaped TextBlob containing glyph positions.
   */
  std::shared_ptr<tgfx::TextBlob> textBlob = nullptr;

  /**
   * Per-glyph anchor offsets relative to default anchor (advance * 0.5, 0).
   */
  std::vector<tgfx::Point> anchors = {};
};

/**
 * Mapping from Text nodes to their shaped text data.
 */
using ShapedTextMap = std::unordered_map<Text*, ShapedText>;

}  // namespace pagx
