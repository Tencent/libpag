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

// Moves the pivot of an affine matrix: T(anchor) * matrix * T(-anchor). Only the translation column
// changes, to t + anchor - L * anchor, so the bottom row - including a homogeneous scale other than
// 1 - is left untouched.
static Matrix3D ConjugateAffine(const Matrix3D& matrix, const Point& anchor, float anchorZ) {
  Matrix3D result = matrix;
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

// Conjugates the affine part of the matrix around the pivot: with matrix = P * A, the result is
// P * T(anchor) * A * T(-anchor). P is the projection built from `perspective`, or the projection
// row split out of a matrix3D; it stays outside the pivot, so the pivot belongs to the object
// transform and the viewer keeps the projection. A layer whose affine part is the identity keeps
// its pose unchanged even under perspective; a 2D matrix is the P = I case and only its
// translation moves.
Matrix3D ApplyAnchorToMatrix(const Matrix3D& matrix, const Point& anchor, float anchorZ) {
  if (anchor.x == 0.0f && anchor.y == 0.0f && anchorZ == 0.0f) {
    return matrix;
  }
  // The bottom row is (0, 0, 0, m33) when P carries no x / y / z projection coefficient, which
  // covers every affine matrix. Conjugating A then leaves the bottom row alone, so the whole matrix
  // is an affine conjugation whatever m33 is.
  const float p0 = matrix.getRowColumn(3, 0);
  const float p1 = matrix.getRowColumn(3, 1);
  const float p2 = matrix.getRowColumn(3, 2);
  if (p0 == 0.0f && p1 == 0.0f && p2 == 0.0f) {
    return ConjugateAffine(matrix, anchor, anchorZ);
  }
  // matrix = P * A with P = [[I, 0], [p0, p1, p2, p3]], so A is exactly the matrix with its bottom
  // row replaced by (0, 0, 0, 1). P.row3.xyz follows from P.row3.xyz * A.linear = matrix.row3.xyz,
  // and the homogeneous scale from p3 = matrix[3][3] - P.row3.xyz . A.translation. Reading p3 out
  // of matrix[3][3] directly would assume p3 = 1 and pull a scaled homogeneous row back to 1.
  const float a0[3] = {matrix.getRowColumn(0, 0), matrix.getRowColumn(0, 1),
                       matrix.getRowColumn(0, 2)};
  const float a1[3] = {matrix.getRowColumn(1, 0), matrix.getRowColumn(1, 1),
                       matrix.getRowColumn(1, 2)};
  const float a2[3] = {matrix.getRowColumn(2, 0), matrix.getRowColumn(2, 1),
                       matrix.getRowColumn(2, 2)};
  const float b[3] = {p0, p1, p2};
  const float det = a0[0] * (a1[1] * a2[2] - a1[2] * a2[1]) -
                    a1[0] * (a0[1] * a2[2] - a0[2] * a2[1]) +
                    a2[0] * (a0[1] * a1[2] - a0[2] * a1[1]);
  if (std::abs(det) < 1e-6f) {
    // A singular linear part cannot be split into a projection and an affine part, so there is no
    // affine part to conjugate. Keeping the input is the only answer that stays finite.
    LOGE("ApplyAnchorToMatrix: ignored the anchor on a singular transform with perspective.");
    return matrix;
  }
  const float solved0 =
      (b[0] * (a1[1] * a2[2] - a1[2] * a2[1]) - a1[0] * (b[1] * a2[2] - b[2] * a2[1]) +
       a2[0] * (b[1] * a1[2] - b[2] * a1[1])) /
      det;
  const float solved1 =
      (a0[0] * (b[1] * a2[2] - b[2] * a2[1]) - b[0] * (a0[1] * a2[2] - a0[2] * a2[1]) +
       a2[0] * (a0[1] * b[2] - a0[2] * b[1])) /
      det;
  const float solved2 =
      (a0[0] * (a1[1] * b[2] - b[1] * a1[2]) - a1[0] * (a0[1] * b[2] - b[1] * a0[2]) +
       b[0] * (a0[1] * a1[2] - a0[2] * a1[1])) /
      det;
  const float p3 = matrix.getRowColumn(3, 3) - solved0 * matrix.getRowColumn(0, 3) -
                   solved1 * matrix.getRowColumn(1, 3) - solved2 * matrix.getRowColumn(2, 3);
  Matrix3D affine = matrix;
  affine.setRowColumn(3, 0, 0.0f);
  affine.setRowColumn(3, 1, 0.0f);
  affine.setRowColumn(3, 2, 0.0f);
  affine.setRowColumn(3, 3, 1.0f);
  const Matrix3D anchored = ConjugateAffine(affine, anchor, anchorZ);
  Matrix3D result = anchored;
  for (int column = 0; column < 4; ++column) {
    result.setRowColumn(3, column,
                        solved0 * anchored.getRowColumn(0, column) +
                            solved1 * anchored.getRowColumn(1, column) +
                            solved2 * anchored.getRowColumn(2, column));
  }
  result.setRowColumn(3, 3, result.getRowColumn(3, 3) + p3);
  return result;
}

Matrix3D ComposeLayerTransform3DWithoutAnchor(const Layer* layer) {
  if (!layer->matrix3D.isIdentity()) {
    if (IsFiniteMatrix3D(layer->matrix3D)) {
      return layer->matrix3D;
    }
    LOGE("BuildLayerTransform3DWithAnchor: ignored a non-finite matrix3D.");
  }
  if (!layer->matrix.isIdentity()) {
    if (IsFiniteMatrix(layer->matrix)) {
      return LiftMatrixToMatrix3D(layer->matrix, layer->z);
    }
    LOGE("BuildLayerTransform3DWithAnchor: ignored a non-finite matrix.");
  }
  Matrix3DComponents components = {};
  Matrix3D result = {};
  LayerTransformComponents(layer, &components, &result);
  return result;
}

Matrix3D BuildLayerTransform3DWithAnchor(const Layer* layer) {
  return ApplyAnchorToMatrix(ComposeLayerTransform3DWithoutAnchor(layer), layer->anchor,
                             layer->anchorZ);
}

}  // namespace pagx
