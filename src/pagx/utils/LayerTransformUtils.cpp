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

namespace pagx {

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
  components.skew[0] = layer->skewX;
  components.skew[1] = layer->skewY;
  components.perspective = layer->perspective;
  Matrix3D matrix = {};
  return ComposeMatrix3DComponents(components, &matrix) ? components : translation;
}

Matrix3D BuildLayerLocalMatrix3D(const Layer* layer) {
  if (!layer->matrix3D.isIdentity()) {
    return layer->matrix3D;
  }
  if (!layer->matrix.isIdentity()) {
    return LiftMatrixToMatrix3D(layer->matrix, layer->z);
  }
  Matrix3D result = {};
  ComposeMatrix3DComponents(LayerTransformComponents(layer), &result);
  return result;
}

}  // namespace pagx
