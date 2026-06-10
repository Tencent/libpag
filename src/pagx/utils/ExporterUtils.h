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

struct FillStrokeInfo {
  const Fill* fill = nullptr;
  const Stroke* stroke = nullptr;
  const TextBox* textBox = nullptr;
};

FillStrokeInfo CollectFillStroke(const std::vector<Element*>& contents);

Matrix BuildLayerMatrix(const Layer* layer);

Matrix BuildGroupMatrix(const Group* group);

FillRule DetectMaskFillRule(const Layer* maskLayer);

/**
 * Decomposes the X and Y scale factors of a 2D affine matrix.  The X scale is the
 * length of the (a, b) basis vector; the Y scale is derived from |det(m)| / sx so
 * that sx * sy equals the absolute area scale of the matrix (and sy stays positive
 * even when the matrix is mirrored).  Returns sx = sy = 0 for degenerate matrices.
 */
void DecomposeScale(const Matrix& m, float* sx, float* sy);

}  // namespace pagx
