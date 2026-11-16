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

#include "pagx/nodes/KeyValue.h"
#include "pagx/nodes/Keyframe.h"
#include "pagx/types/Point.h"

namespace pagx {

/**
 * Evaluates the easing fraction of one keyframe segment. This is the single easing entry shared by
 * playback (EvaluateKeyframeSequence), segment sampling and state-machine crossfades, so a curve
 * cannot diverge between the canvas, the curve editor and exported frames.
 *
 * None and Hold return 0 (the segment start). Linear returns progress unchanged and may
 * extrapolate outside [0, 1]. Bezier and Spring clamp progress to [0, 1]; their result may lie
 * outside [0, 1] to express overshoot.
 *
 * @param interpolation the interpolation mode from the segment's left keyframe.
 * @param bezierOut the outgoing bezier handle of the left keyframe.
 * @param bezierIn the incoming bezier handle of the right keyframe.
 * @param spring the spring parameters of the left keyframe.
 * @param progress the normalized position within the segment.
 * @param segmentDurationSeconds the segment length in seconds, used by physical springs. Values
 *        less than or equal to zero make a physical spring fall back to the normalized model
 *        with the damping ratio derived from its physical parameters.
 * @return the eased fraction used to interpolate the segment's values.
 */
double EvaluateKeyframeInterpolationProgress(KeyframeInterpolationType interpolation,
                                             const Point& bezierOut, const Point& bezierIn,
                                             const SpringEasing& spring, double progress,
                                             double segmentDurationSeconds = 0.0);

/**
 * Evaluates the interpolated value between two adjacent keyframes at a given point within their
 * span. The caller computes `progress = (framePosition - keyIn.time) / (keyOut.time - keyIn.time)`
 * and passes it in; this function only applies the interpolation behavior. The caller is
 * expected to pass `progress` in [0, 1]; for Hold, Bezier and Spring modes, out-of-range values
 * are clamped to the boundaries, while Linear may extrapolate outside that range.
 *
 * Supported KeyValue types: float, bool, int, std::string, ImageRef, Color, Matrix, PAGImage.
 *
 * @param leftValue the value at keyIn.
 * @param rightValue the value at keyOut.
 * @param progress the normalized position within the segment.
 * @param interp the interpolation mode from keyIn.
 * @param bezierOut the outgoing bezier handle from keyIn, used when interp is Bezier.
 * @param bezierIn the incoming bezier handle from keyOut, used when interp is Bezier.
 * @param spring the spring parameters from keyIn, used when interp is Spring. A null pointer
 *        selects the default critically damped spring.
 * @param segmentDurationSeconds the segment length in seconds, used by physical springs.
 * @return the interpolated KeyValue at the given position.
 */
KeyValue EvaluateKeyframeSegment(const KeyValue& leftValue, const KeyValue& rightValue,
                                 double progress, KeyframeInterpolationType interp,
                                 const Point* bezierOut = nullptr, const Point* bezierIn = nullptr,
                                 const SpringEasing* spring = nullptr,
                                 double segmentDurationSeconds = 0.0);

}  // namespace pagx
