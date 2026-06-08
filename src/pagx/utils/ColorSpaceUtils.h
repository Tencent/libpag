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

#include "pagx/types/Color.h"
#include "pagx/types/ColorSpace.h"

namespace pagx {

// Converts a color into the target color space, applying the gamut transform between the source and
// target gamuts. The alpha channel is preserved. Returns the input unchanged when it is already in
// the target space. Used to bring two colors into a common space before component interpolation.
Color ConvertColorSpace(const Color& color, ColorSpace target);

// Linearly interpolates between two colors by fraction t, always converging onto the start color's
// space: when the endpoints differ, b is converted into a's space before blending and the result
// stays in a.colorSpace. Both the runtime keyframe evaluator and the HTML exporter route through
// this single helper so animation and export agree on color-space convergence direction.
Color LerpColor(const Color& a, const Color& b, double t);

}  // namespace pagx
