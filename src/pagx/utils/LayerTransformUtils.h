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

#include "pagx/nodes/Layer.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/Matrix3D.h"

namespace pagx {

struct Matrix3DComponents;

// Lifts a 2D affine matrix into a column-major 4x4 matrix that also translates by z along the Z
// axis, leaving the Z axis otherwise untouched.
Matrix3D LiftMatrixToMatrix3D(const Matrix& matrix, float z);

// Returns true if all values of the 2D matrix are finite.
bool IsFiniteMatrix(const Matrix& matrix);

// Returns true if constraint or container layout generated a position for the layer. A generated
// position is applied outside the layer's local transform, as T(layoutPosition) * local.
bool HasGeneratedLayoutPosition(const Layer* layer);

// Returns true if any Layer transform component (rotation, scale, skew or perspective) differs from
// its default value.
bool HasLayerTransformComponents(const Layer* layer);

// Writes the Layer's x / y / z translation and transform components to components and their
// composition to matrix. When the components cannot be composed (a skew of +/-90 degrees or a
// negative perspective), only the translation is kept; matrix is the identity if even that fails.
void LayerTransformComponents(const Layer* layer, Matrix3DComponents* components, Matrix3D* matrix);

// Returns the Layer's local transform without the layout position: the non-identity matrix3D, else
// the non-identity matrix lifted with z, else the composition of x / y / z and the components.
Matrix3D BuildLayerLocalMatrix3D(const Layer* layer);

}  // namespace pagx
