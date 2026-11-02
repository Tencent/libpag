/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/html/importer/HTMLBoxAttributes.h"

namespace pagx {

class HTMLValueParser;

struct HTMLBackgroundRect {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
};

struct HTMLBackgroundAxis {
  float size = 0;
  float first = 0;
  float step = 0;
  int count = 0;
  bool contiguous = false;
};

struct HTMLBackgroundLayout {
  HTMLBackgroundRect origin;
  HTMLBackgroundRect clip;
  HTMLBackgroundAxis x;
  HTMLBackgroundAxis y;
};

// A shorter CSS list cycles, including entries corresponding to `none` images.
std::string BackgroundListEntry(const std::string& value, size_t index, const std::string& initial);
HTMLBackgroundRect BackgroundBoxRect(const HTMLBoxAttributes& box, const std::string& keyword);

// Resolves CSS <position> against (area - image), or against area for a zero-size point.
bool ResolveBackgroundPosition(const std::string& value, float slackX, float slackY,
                               HTMLValueParser& parser, float& x, float& y);

// No intrinsic dimensions means a generated image, not an image with the box's aspect ratio.
// Returns false for unsupported/invalid syntax; zero-sized tiles are valid and have zero copies.
bool ResolveBackgroundLayout(const HTMLBoxAttributes& box, size_t index, float intrinsicWidth,
                             float intrinsicHeight, HTMLValueParser& parser,
                             HTMLBackgroundLayout& result);

}  // namespace pagx
