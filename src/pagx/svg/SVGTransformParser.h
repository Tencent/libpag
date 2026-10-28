/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include "pagx/types/Matrix.h"

namespace pagx {

/**
 * Parses an SVG transform list (translate, scale, rotate with an optional pivot, skewX, skewY and
 * matrix functions) into a single 2D affine matrix, composing the functions left to right as SVG
 * does. Missing numbers read as zero and parsing stops at the first malformed function; an empty
 * string yields the identity matrix.
 */
Matrix MatrixFromSVGTransform(const std::string& value);

}  // namespace pagx
