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

#pragma once

#include "LayerFilter.h"

namespace pag {
class GradientOverlayFilter : public LayerFilter {
 public:
  explicit GradientOverlayFilter(GradientOverlayStyle* layerStyle);
  ~GradientOverlayFilter() override = default;

 protected:
  std::string onBuildFragmentShader() override;

  void onPrepareProgram(tgfx::Context* context, unsigned program) override;

  void onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                      const tgfx::Point& filterScale) override;

 private:
  GradientOverlayStyle* layerStyle = nullptr;
  unsigned gradientOverlayProgram = 0;
  
  int opacityHandle = 0;
  int colorSizeHandle = 0;
  int alphaSizeHandle = 0;
  int angleHandle = 0;
  int styleHandle = 0;
  int scaleHandle = 0;
  int offsetHandle = 0;
  int sizeHandle = 0;
};
}  // namespace pag
