/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

#include "pagx/utils/ExporterUtils.h"
#include <cmath>
#include "base/utils/MathUtil.h"

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyZero;

FillStrokeInfo CollectFillStroke(const std::vector<Element*>& contents) {
  FillStrokeInfo info = {};
  for (const auto* element : contents) {
    if (element->nodeType() == NodeType::Fill && !info.fill) {
      info.fill = static_cast<const Fill*>(element);
    } else if (element->nodeType() == NodeType::Stroke && !info.stroke) {
      info.stroke = static_cast<const Stroke*>(element);
    } else if (element->nodeType() == NodeType::TextBox && !info.textBox) {
      info.textBox = static_cast<const TextBox*>(element);
    }
    if (info.fill && info.stroke && info.textBox) {
      break;
    }
  }
  return info;
}

Matrix BuildLayerMatrix(const Layer* layer) {
  // matrix3D > matrix > renderPosition. When matrix3D is non-identity it overrides
  // both matrix and renderPosition. We project it onto the z=0 plane to get the
  // equivalent 2D affine transform used by the 2D exporters (PPT/SVG).
  if (!layer->matrix3D.isIdentity()) {
    const float* v = layer->matrix3D.values;
    Matrix m = {};
    m.a = v[0];
    m.b = v[1];
    m.c = v[4];
    m.d = v[5];
    m.tx = v[12];
    m.ty = v[13];
    return m;
  }
  Matrix m = layer->matrix;
  // T(px,py) * m only shifts the translation column, so apply it directly to
  // avoid the full 6-multiply Matrix::operator* dance for the common case
  // where the layer's render position carries the translation. The authored
  // x/y attributes are layout inputs only; the layout pass resolves them
  // (together with constraints like left/right/top/bottom, flex, padding) into
  // renderPosition(), which is what the renderer uses.
  auto renderPos = layer->renderPosition();
  m.tx += renderPos.x;
  m.ty += renderPos.y;
  return m;
}

Matrix BuildGroupMatrix(const Group* group) {
  // The authored `position` attribute is a layout input only; the actual
  // rendered translation comes from renderPosition() after constraints,
  // padding, and flex allocation have been resolved. anchor, rotation,
  // scale, and skew are not layout-owned and are read directly.
  auto renderPos = group->renderPosition();
  bool hasAnchor = !FloatNearlyZero(group->anchor.x) || !FloatNearlyZero(group->anchor.y);
  bool hasPosition = !FloatNearlyZero(renderPos.x) || !FloatNearlyZero(renderPos.y);
  bool hasRotation = !FloatNearlyZero(group->rotation);
  bool hasScale =
      !FloatNearlyZero(group->scale.x - 1.0f) || !FloatNearlyZero(group->scale.y - 1.0f);
  bool hasSkew = !FloatNearlyZero(group->skew);

  if (!hasAnchor && !hasPosition && !hasRotation && !hasScale && !hasSkew) {
    return {};
  }

  Matrix m = {};
  if (hasAnchor) {
    m = Matrix::Translate(-group->anchor.x, -group->anchor.y);
  }
  if (hasScale) {
    m = Matrix::Scale(group->scale.x, group->scale.y) * m;
  }
  if (hasSkew) {
    // Sign matches the native renderer (ShapeRenderer::SkewFromAxis passes
    // DegreesToRadians(-skew)). Keeping a single convention across native, SVG,
    // PPT, and HTML exporters avoids per-pipeline sign flips.
    m = Matrix::Rotate(group->skewAxis) * m;
    Matrix shear = {};
    shear.c = std::tan(DegreesToRadians(-group->skew));
    m = shear * m;
    m = Matrix::Rotate(-group->skewAxis) * m;
  }
  if (hasRotation) {
    m = Matrix::Rotate(group->rotation) * m;
  }
  if (hasPosition) {
    m = Matrix::Translate(renderPos.x, renderPos.y) * m;
  }

  return m;
}

FillRule DetectMaskFillRule(const Layer* maskLayer) {
  for (const auto* element : maskLayer->contents) {
    if (element->nodeType() == NodeType::Fill) {
      auto rule = static_cast<const Fill*>(element)->fillRule;
      if (rule == FillRule::EvenOdd) {
        return FillRule::EvenOdd;
      }
    }
  }
  for (const auto* child : maskLayer->children) {
    if (DetectMaskFillRule(child) == FillRule::EvenOdd) {
      return FillRule::EvenOdd;
    }
  }
  return FillRule::Winding;
}

void DecomposeScale(const Matrix& m, float* sx, float* sy) {
  *sx = std::sqrt(m.a * m.a + m.b * m.b);
  float det = m.a * m.d - m.b * m.c;
  *sy = (*sx > 0) ? std::abs(det) / *sx : 0;
}

}  // namespace pagx
