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

#include <string>
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

/**
 * Walks `contents` (and recurses through Group / TextBox containers) and returns true as soon as
 * a Fill or Stroke painter with placement == Foreground is found. The exporters use this to
 * decide whether a second painter pass is needed: skipping the foreground pass entirely when no
 * foreground painter exists keeps simple layers terse, but a foreground painter buried inside a
 * Group must still be detected, otherwise its background-pass output is suppressed by the
 * placement filter and its foreground-pass output is suppressed by the missing dispatch.
 */
bool HasForegroundPainter(const std::vector<Element*>& contents);

/**
 * Escapes a font-family value for safe emission inside a CSS declaration wrapped in single
 * quotes (e.g. `font-family:'<escaped>'`) or inside an SVG attribute that already wraps the
 * family name in quotes. Drops control characters and CSS-significant characters that would
 * otherwise let the value escape its quoted context, and backslash-escapes any embedded
 * single-quote / backslash. Empty input yields an empty output.
 *
 * Both HTML and SVG exporters call this so the two pipelines emit identical strings for
 * identical input — the @font-face declaration in particular must match the family-name
 * spelling on every <text>/<span> reference, otherwise the browser cannot resolve the font.
 */
std::string EscapeCssFontFamily(const std::string& family);

/**
 * Escapes a font-family value for safe emission inside a CSS declaration wrapped in single
 * quotes (e.g. `font-family:'<escaped>'`) or inside an SVG attribute that already wraps the
 * family name in quotes. Drops control characters and CSS-significant characters that would
 * otherwise let the value escape its quoted context, and backslash-escapes any embedded
 * single-quote / backslash. Empty input yields an empty output.
 *
 * Both HTML and SVG exporters call this so the two pipelines emit identical strings for
 * identical input — the @font-face declaration in particular must match the family-name
 * spelling on every <text>/<span> reference, otherwise the browser cannot resolve the font.
 */
std::string EscapeCssFontFamily(const std::string& family);

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
