/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "CGTypeface.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/PixelBuffer.h"

namespace tgfx {
class CGScalerContextRec {
 public:
  float textSize = 12.f;
  float skewX = 0.f;
  float fauxBoldSize = 0.f;
  bool verticalText = false;
};

class CGScalerContext {
 public:
  static std::unique_ptr<CGScalerContext> Make(std::shared_ptr<Typeface> typeface, float size,
                                               bool fauxBold = false, bool fauxItalic = false,
                                               bool verticalText = false);
  ~CGScalerContext();

  FontMetrics generateFontMetrics();

  GlyphMetrics generateGlyphMetrics(GlyphID glyphID);

  Point getGlyphVerticalOffset(GlyphID glyphID) const;

  bool generatePath(GlyphID glyphID, Path* path);

  std::shared_ptr<TextureBuffer> generateImage(GlyphID glyphID, Matrix* matrix);

 private:
  CGScalerContext(std::shared_ptr<Typeface> typeface, CGScalerContextRec);

  std::shared_ptr<Typeface> _typeface;
  CGScalerContextRec rec;
  CTFontRef ctFont = nullptr;
  CGAffineTransform transform = CGAffineTransformIdentity;
};
}  // namespace tgfx
