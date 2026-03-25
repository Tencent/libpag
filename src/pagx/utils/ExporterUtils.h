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
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/types/Matrix.h"

namespace pagx {

class PathData;
class Text;

struct FillStrokeInfo {
  const Fill* fill = nullptr;
  const Stroke* stroke = nullptr;
  const TextBox* textBox = nullptr;
};

struct GlyphPath {
  Matrix transform;
  const PathData* pathData;
};

FillStrokeInfo CollectFillStroke(const std::vector<Element*>& contents);

Matrix BuildLayerMatrix(const Layer* layer);

Matrix BuildGroupMatrix(const Group* group);

/**
 * Converts text glyph runs into a list of glyph paths with transform matrices.
 * textPosX/textPosY are the base position for glyph placement.
 */
std::vector<GlyphPath> ComputeGlyphPaths(const Text& text, float textPosX, float textPosY);

}  // namespace pagx
