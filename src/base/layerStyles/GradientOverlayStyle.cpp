///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  The MIT License (MIT)
//
//  Copyright (c) 2016-present, Tencent. All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
//  and associated documentation files (the "Software"), to deal in the Software without
//  restriction, including without limitation the rights to use, copy, modify, merge, publish,
//  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//      The above copyright notice and this permission notice shall be included in all copies or
//      substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
//  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "base/utils/MathUtil.h"
#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
GradientOverlayStyle::~GradientOverlayStyle() {
  delete blendMode;
  delete opacity;
  delete colors;
  delete gradientSmoothness;
  delete angle;
  delete style;
  delete reverse;
  delete alignWithLayer;
  delete scale;
  delete offset;
}

bool GradientOverlayStyle::visibleAt(Frame) const {
  return true;
}

void GradientOverlayStyle::transformBounds(Rect*, const Point&, Frame) const {
}

void GradientOverlayStyle::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  blendMode->excludeVaryingRanges(timeRanges);
  opacity->excludeVaryingRanges(timeRanges);
  colors->excludeVaryingRanges(timeRanges);
  gradientSmoothness->excludeVaryingRanges(timeRanges);
  angle->excludeVaryingRanges(timeRanges);
  style->excludeVaryingRanges(timeRanges);
  reverse->excludeVaryingRanges(timeRanges);
  alignWithLayer->excludeVaryingRanges(timeRanges);
  scale->excludeVaryingRanges(timeRanges);
  offset->excludeVaryingRanges(timeRanges);
}

bool GradientOverlayStyle::verify() const {
  if (!LayerStyle::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(blendMode != nullptr && opacity != nullptr && colors != nullptr &&
                  gradientSmoothness != nullptr && angle != nullptr && style != nullptr &&
                  reverse != nullptr && alignWithLayer != nullptr && scale != nullptr &&
                  offset != nullptr);
}
}  // namespace pag
