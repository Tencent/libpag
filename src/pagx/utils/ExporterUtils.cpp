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

bool HasForegroundPainter(const std::vector<Element*>& contents) {
  for (const auto* element : contents) {
    auto type = element->nodeType();
    if (type == NodeType::Fill) {
      if (static_cast<const Fill*>(element)->placement == LayerPlacement::Foreground) {
        return true;
      }
    } else if (type == NodeType::Stroke) {
      if (static_cast<const Stroke*>(element)->placement == LayerPlacement::Foreground) {
        return true;
      }
    } else if (type == NodeType::TextBox) {
      // TextBox derives from Group; check its child elements before treating it as a leaf.
      auto* tb = static_cast<const TextBox*>(element);
      if (HasForegroundPainter(tb->elements)) {
        return true;
      }
    } else if (type == NodeType::Group) {
      auto* group = static_cast<const Group*>(element);
      if (HasForegroundPainter(group->elements)) {
        return true;
      }
    }
  }
  return false;
}

std::string EscapeCssFontFamily(const std::string& family) {
  std::string out;
  out.reserve(family.size());
  for (char c : family) {
    // Drop control characters (including NUL, \n, \r, \t) and the characters that would let an
    // attacker escape the single-quoted font-family value into the surrounding CSS context.
    auto uc = static_cast<unsigned char>(c);
    if (uc < 0x20 || c == ';' || c == '}' || c == '{' || c == '<' || c == '>') {
      continue;
    }
    if (c == '\\' || c == '\'') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

bool HasLayerTransformComponents(const Layer* layer) {
  return layer->rotationX != 0.0f || layer->rotationY != 0.0f || layer->rotationZ != 0.0f ||
         layer->scaleX != 1.0f || layer->scaleY != 1.0f || layer->scaleZ != 1.0f ||
         layer->skewXY != 0.0f || layer->skewXZ != 0.0f || layer->skewYZ != 0.0f ||
         layer->perspective != 0.0f;
}

Matrix3DComponents LayerTransformComponents(const Layer* layer) {
  Matrix3DComponents components = {};
  components.translate[0] = layer->x;
  components.translate[1] = layer->y;
  components.translate[2] = layer->z;
  Matrix3DComponents translation = components;
  components.rotation[0] = layer->rotationX;
  components.rotation[1] = layer->rotationY;
  components.rotation[2] = layer->rotationZ;
  components.scale[0] = layer->scaleX;
  components.scale[1] = layer->scaleY;
  components.scale[2] = layer->scaleZ;
  components.skew[0] = layer->skewXY;
  components.skew[1] = layer->skewXZ;
  components.skew[2] = layer->skewYZ;
  components.perspective = layer->perspective;
  Matrix3D matrix = {};
  return ComposeMatrix3DComponents(components, &matrix) ? components : translation;
}

Matrix3D BuildLayerLocalMatrix3D(const Layer* layer) {
  if (!layer->matrix3D.isIdentity()) {
    return layer->matrix3D;
  }
  Matrix3D result = {};
  if (!layer->matrix.isIdentity()) {
    const auto& m = layer->matrix;
    result.setRowColumn(0, 0, m.a);
    result.setRowColumn(0, 1, m.c);
    result.setRowColumn(0, 3, m.tx);
    result.setRowColumn(1, 0, m.b);
    result.setRowColumn(1, 1, m.d);
    result.setRowColumn(1, 3, m.ty);
    result.setRowColumn(2, 3, layer->z);
    return result;
  }
  ComposeMatrix3DComponents(LayerTransformComponents(layer), &result);
  return result;
}

Matrix BuildLayerMatrix(const Layer* layer) {
  // Local transform priority: matrix3D > matrix > x / y / z and the transform components. The
  // position generated by constraints or container layout stays outside the local transform:
  // T(layout) * local. A 3D local transform is projected onto the z=0 plane as a 2D affine, and the
  // layout translation is applied after the perspective divide, so it only shifts the projected
  // translation.
  auto layoutPosition = layer->layoutPosition();
  float layoutX = std::isnan(layoutPosition.x) ? 0 : layoutPosition.x;
  float layoutY = std::isnan(layoutPosition.y) ? 0 : layoutPosition.y;
  Matrix m = layer->matrix;
  if (!layer->matrix3D.isIdentity() ||
      (layer->matrix.isIdentity() && HasLayerTransformComponents(layer))) {
    auto local = BuildLayerLocalMatrix3D(layer);
    const float* v = local.values;
    m.a = v[0];
    m.b = v[1];
    m.c = v[4];
    m.d = v[5];
    m.tx = v[12];
    m.ty = v[13];
  } else if (layer->matrix.isIdentity()) {
    m.tx += layer->x;
    m.ty += layer->y;
  }
  m.tx += layoutX;
  m.ty += layoutY;
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
