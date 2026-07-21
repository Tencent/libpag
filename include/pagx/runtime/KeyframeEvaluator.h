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

#include <cmath>
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Keyframe.h"
#include "pagx/types/Point.h"

namespace pagx {

/**
 * Evaluates the interpolated value between two adjacent keyframes at a given point within their
 * span. The caller computes `rawT = (framePosition - keyIn.time) / (keyOut.time - keyIn.time)`
 * and passes it in; this function only applies the interpolation behavior.
 *
 * Supported KeyValue types: float, bool, int, std::string, ImageRef, Color, Matrix, PAGImage.
 *
 * @param leftValue the value at keyIn.
 * @param rightValue the value at keyOut.
 * @param interp the interpolation mode from keyIn.
 * @param bezierOut the outgoing bezier handle from keyIn, used when interp is Bezier.
 * @param bezierIn the incoming bezier handle from keyOut, used when interp is Bezier.
 * @param rawT the normalized position in [0, 1] within the segment.
 * @return the interpolated KeyValue at the given position.
 */
KeyValue EvaluateKeyframeSegment(const KeyValue& leftValue,
                                 const KeyValue& rightValue,
                                 KeyframeInterpolationType interp,
                                 const Point* bezierOut, const Point* bezierIn,
                                 double rawT);

}  // namespace pagx
