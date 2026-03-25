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
#include "pagx/types/Matrix.h"

namespace pagx {

class PathData;
class Text;

struct GlyphPath {
  Matrix transform;
  const PathData* pathData;
};

/**
 * Converts text glyph runs into a list of glyph paths with transform matrices.
 * textPosX/textPosY are the base position for glyph placement.
 */
std::vector<GlyphPath> ComputeGlyphPaths(const Text& text, float textPosX, float textPosY);

}  // namespace pagx
