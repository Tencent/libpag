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

#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
HueSaturationEffect::~HueSaturationEffect() {
  delete colorizeHue;
  delete colorizeSaturation;
  delete colorizeLightness;
}

bool HueSaturationEffect::visibleAt(Frame) const {
  return true;
}

void HueSaturationEffect::transformBounds(Rect*, const Point&, Frame) const {}

void HueSaturationEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
  Effect::excludeVaryingRanges(timeRanges);
  if (colorize) {
    colorizeHue->excludeVaryingRanges(timeRanges);
    colorizeSaturation->excludeVaryingRanges(timeRanges);
    colorizeLightness->excludeVaryingRanges(timeRanges);
  }
}

bool HueSaturationEffect::verify() const {
  if (!Effect::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(colorizeHue != nullptr && colorizeSaturation != nullptr && colorizeLightness != nullptr);
}
}  // namespace pag
