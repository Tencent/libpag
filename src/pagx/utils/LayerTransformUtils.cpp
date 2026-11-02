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

#include "pagx/utils/LayerTransformUtils.h"
#include <cmath>
#include <cstdio>
#include "base/utils/Log.h"
#include "pagx/runtime/Matrix3DDecompose.h"

namespace pagx {

bool IsFiniteMatrix(const Matrix& matrix) {
  return std::isfinite(matrix.a) && std::isfinite(matrix.b) && std::isfinite(matrix.c) &&
         std::isfinite(matrix.d) && std::isfinite(matrix.tx) && std::isfinite(matrix.ty);
}

bool HasPerspective(const Matrix3D& matrix) {
  return matrix.getRowColumn(3, 0) != 0 || matrix.getRowColumn(3, 1) != 0 ||
         matrix.getRowColumn(3, 2) != 0;
}

Matrix3D LiftMatrixToMatrix3D(const Matrix& matrix, float z) {
  Matrix3D result = {};
  result.setRowColumn(0, 0, matrix.a);
  result.setRowColumn(0, 1, matrix.c);
  result.setRowColumn(0, 3, matrix.tx);
  result.setRowColumn(1, 0, matrix.b);
  result.setRowColumn(1, 1, matrix.d);
  result.setRowColumn(1, 3, matrix.ty);
  result.setRowColumn(2, 3, z);
  return result;
}

bool HasLayer3DTransform(const Layer* layer) {
  return layer->z != 0.0f || !layer->matrix3D.isIdentity() || HasLayerTransformComponents(layer);
}

bool HasLayerTransformComponents(const Layer* layer) {
  return layer->rotationX != 0.0f || layer->rotationY != 0.0f || layer->rotationZ != 0.0f ||
         layer->scaleX != 1.0f || layer->scaleY != 1.0f || layer->scaleZ != 1.0f ||
         layer->skewX != 0.0f || layer->skewY != 0.0f || layer->perspective != 0.0f;
}

void LayerTransformComponents(const Layer* layer, Matrix3DComponents* components,
                              Matrix3D* matrix) {
  components->translate[0] = layer->x;
  components->translate[1] = layer->y;
  components->translate[2] = layer->z;
  components->rotation[0] = layer->rotationX;
  components->rotation[1] = layer->rotationY;
  components->rotation[2] = layer->rotationZ;
  components->scale[0] = layer->scaleX;
  components->scale[1] = layer->scaleY;
  components->scale[2] = layer->scaleZ;
  components->skew[0] = layer->skewX;
  components->skew[1] = layer->skewY;
  components->perspective = layer->perspective;
  *matrix = {};
  if (ComposeMatrix3DComponents(*components, matrix)) {
    return;
  }
  LOGE("LayerTransformComponents: fell back to the translation only.");
  Matrix3DComponents translation = {};
  for (int i = 0; i < 3; ++i) {
    translation.translate[i] = components->translate[i];
  }
  *components = translation;
  *matrix = {};
  ComposeMatrix3DComponents(*components, matrix);
}

// Moves the transform pivot to `anchor`: T(anchor) * M * T(-anchor). The linear part is unchanged
// and the translation becomes t + anchor - L * anchor, so the translation stays outside the pivot.
Matrix3D ApplyAnchorToMatrix(const Matrix3D& matrix, const Point& anchor, float anchorZ) {
  if (anchor.x == 0.0f && anchor.y == 0.0f && anchorZ == 0.0f) {
    return matrix;
  }
  Matrix3D result = matrix;
  const bool hasPerspective =
      matrix.getRowColumn(3, 0) != 0.0f || matrix.getRowColumn(3, 1) != 0.0f ||
      matrix.getRowColumn(3, 2) != 0.0f || matrix.getRowColumn(3, 3) != 1.0f;
  if (hasPerspective) {
    // A perspective matrix is P * A with P built from the bottom row (the projection) and A affine.
    // The pivot belongs to the affine chain only, mirroring how DCC tools keep the pivot inside the
    // object transform and apply the camera last: M = P * T(a) * A * T(-a). Touching the translation
    // column alone would leave the w row inconsistent, hence the split.
    // Recover P's bottom row from M = P * A: M.row3.xyz = p0*A.row0 + p1*A.row1 + p2*A.row2 with
    // A.row3 = (0,0,0,1). Taking M.row3 directly as P would lose the part contributed by A.
    const float a0[3] = {matrix.getRowColumn(0, 0), matrix.getRowColumn(0, 1),
                         matrix.getRowColumn(0, 2)};
    const float a1[3] = {matrix.getRowColumn(1, 0), matrix.getRowColumn(1, 1),
                         matrix.getRowColumn(1, 2)};
    const float a2[3] = {matrix.getRowColumn(2, 0), matrix.getRowColumn(2, 1),
                         matrix.getRowColumn(2, 2)};
    const float b[3] = {matrix.getRowColumn(3, 0), matrix.getRowColumn(3, 1),
                        matrix.getRowColumn(3, 2)};
    const float det = a0[0] * (a1[1] * a2[2] - a1[2] * a2[1]) -
                      a1[0] * (a0[1] * a2[2] - a0[2] * a2[1]) +
                      a2[0] * (a0[1] * a1[2] - a0[2] * a1[1]);
    if (std::abs(det) < 1e-6f) {
      return matrix;
    }
    const float p0 =
        (b[0] * (a1[1] * a2[2] - a1[2] * a2[1]) - a1[0] * (b[1] * a2[2] - b[2] * a2[1]) +
         a2[0] * (b[1] * a1[2] - b[2] * a1[1])) /
        det;
    const float p1 =
        (a0[0] * (b[1] * a2[2] - b[2] * a2[1]) - b[0] * (a0[1] * a2[2] - a0[2] * a2[1]) +
         a2[0] * (a0[1] * b[2] - a0[2] * b[1])) /
        det;
    const float p2 =
        (a0[0] * (a1[1] * b[2] - b[1] * a1[2]) - a1[0] * (a0[1] * b[2] - b[1] * a0[2]) +
         b[0] * (a0[1] * a1[2] - a0[2] * a1[1])) /
        det;
    Matrix3D affine = matrix;
    affine.setRowColumn(3, 0, 0.0f);
    affine.setRowColumn(3, 1, 0.0f);
    affine.setRowColumn(3, 2, 0.0f);
    affine.setRowColumn(3, 3, 1.0f);
    const Matrix3D anchored = ApplyAnchorToMatrix(affine, anchor, anchorZ);
    for (int row = 0; row < 3; ++row) {
      for (int column = 0; column < 4; ++column) {
        result.setRowColumn(row, column, anchored.getRowColumn(row, column));
      }
    }
    for (int column = 0; column < 4; ++column) {
      result.setRowColumn(
          3, column,
          p0 * anchored.getRowColumn(0, column) + p1 * anchored.getRowColumn(1, column) +
              p2 * anchored.getRowColumn(2, column) + anchored.getRowColumn(3, column));
    }
    return result;
  }
  const float linearX = matrix.getRowColumn(0, 0) * anchor.x +
                        matrix.getRowColumn(0, 1) * anchor.y + matrix.getRowColumn(0, 2) * anchorZ;
  const float linearY = matrix.getRowColumn(1, 0) * anchor.x +
                        matrix.getRowColumn(1, 1) * anchor.y + matrix.getRowColumn(1, 2) * anchorZ;
  const float linearZ = matrix.getRowColumn(2, 0) * anchor.x +
                        matrix.getRowColumn(2, 1) * anchor.y + matrix.getRowColumn(2, 2) * anchorZ;
  result.setRowColumn(0, 3, matrix.getRowColumn(0, 3) + anchor.x - linearX);
  result.setRowColumn(1, 3, matrix.getRowColumn(1, 3) + anchor.y - linearY);
  result.setRowColumn(2, 3, matrix.getRowColumn(2, 3) + anchorZ - linearZ);
  return result;
}

Matrix3D ComposeLayerLocalMatrix3D(const Layer* layer) {
  if (!layer->matrix3D.isIdentity()) {
    if (IsFiniteMatrix3D(layer->matrix3D)) {
      return layer->matrix3D;
    }
    LOGE("BuildLayerLocalMatrix3D: ignored a non-finite matrix3D.");
  }
  if (!layer->matrix.isIdentity()) {
    if (IsFiniteMatrix(layer->matrix)) {
      return LiftMatrixToMatrix3D(layer->matrix, layer->z);
    }
    LOGE("BuildLayerLocalMatrix3D: ignored a non-finite matrix.");
  }
  Matrix3DComponents components = {};
  Matrix3D result = {};
  LayerTransformComponents(layer, &components, &result);
  return result;
}

Matrix3D BuildLayerLocalMatrix3D(const Layer* layer) {
  const bool hasAnchor =
      layer->anchor.x != 0.0f || layer->anchor.y != 0.0f || layer->anchorZ != 0.0f;
  if (!hasAnchor) {
    return ComposeLayerLocalMatrix3D(layer);
  }
  // Component path: the composition puts the perspective matrix outermost (M = P * T * R * K * S),
  // so the pivot only belongs to the affine chain: M = P * T(a) * L * T(-a). Building the affine
  // part without perspective, folding the anchor there and recomposing keeps the projection fixed
  // while the pivot still decides the visual centre.
  if (layer->matrix3D.isIdentity() && layer->matrix.isIdentity()) {
    Matrix3DComponents components = {};
    Matrix3D composed = {};
    LayerTransformComponents(layer, &components, &composed);
    Matrix3DComponents flat = components;
    flat.perspective = 0;
    Matrix3D affine = {};
    if (ComposeMatrix3DComponents(flat, &affine)) {
      const Matrix3D anchored = ApplyAnchorToMatrix(affine, layer->anchor, layer->anchorZ);
      components.translate[0] = anchored.getRowColumn(0, 3);
      components.translate[1] = anchored.getRowColumn(1, 3);
      components.translate[2] = anchored.getRowColumn(2, 3);
      if (ComposeMatrix3DComponents(components, &composed)) {
        return composed;
      }
    }
  }
  return ApplyAnchorToMatrix(ComposeLayerLocalMatrix3D(layer), layer->anchor, layer->anchorZ);
}

}  // namespace pagx
