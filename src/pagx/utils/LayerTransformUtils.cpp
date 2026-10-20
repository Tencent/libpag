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
#include "base/utils/Log.h"
#include "pagx/runtime/Matrix3DDecompose.h"

namespace pagx {

bool IsFiniteMatrix(const Matrix& matrix) {
  return std::isfinite(matrix.a) && std::isfinite(matrix.b) && std::isfinite(matrix.c) &&
         std::isfinite(matrix.d) && std::isfinite(matrix.tx) && std::isfinite(matrix.ty);
}

bool HasPerspective(const Matrix3D& matrix) {
  return matrix.getRowColumn(3, 0) != 0 || matrix.getRowColumn(3, 1) != 0 ||
         matrix.getRowColumn(3, 2) != 0 || matrix.getRowColumn(3, 3) != 1;
}

bool HasGeneratedLayoutPosition(const Layer* layer) {
  if (layer == nullptr) {
    return false;
  }
  auto position = layer->layoutPosition();
  return !std::isnan(position.x) || !std::isnan(position.y);
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

Matrix3D BuildLayerLocalMatrix3D(const Layer* layer) {
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

}  // namespace pagx
