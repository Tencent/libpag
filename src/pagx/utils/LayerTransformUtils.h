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
#include "pagx/runtime/Matrix3DDecompose.h"
#include "pagx/types/Matrix3D.h"

namespace pagx {

// Returns true if any Layer transform component (rotation, scale, skew or perspective) differs from
// its default value.
bool HasLayerTransformComponents(const Layer* layer);

// Returns the Layer's x / y / z translation and transform components. When the components cannot be
// composed (a skew of +/-90 degrees or a negative perspective), only the translation is kept.
Matrix3DComponents LayerTransformComponents(const Layer* layer);

// Returns the Layer's local transform without the layout position: the non-identity matrix3D, else
// the non-identity matrix lifted with z, else the composition of x / y / z and the components.
Matrix3D BuildLayerLocalMatrix3D(const Layer* layer);

}  // namespace pagx
