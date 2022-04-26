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

#include <memory>
#include "ft2build.h"
#include FT_FREETYPE_H
#include "FTFace.h"
#include "FTTypeface.h"
#include "tgfx/core/PixelBuffer.h"

namespace tgfx {
class FTScalerContextRec {
 public:
  bool computeMatrices(Point* scale, Matrix* remaining) const;

  float textSize = 12.f;
  float skewX = 0.0f;
  bool embolden = false;
  bool verticalText = false;
};

class FTScalerContext {
 public:
  static std::unique_ptr<FTScalerContext> Make(std::shared_ptr<Typeface> typeface, float size,
                                               bool fauxBold = false, bool fauxItalic = false,
                                               bool verticalText = false);

  ~FTScalerContext();

  FontMetrics generateFontMetrics();

  GlyphMetrics generateGlyphMetrics(GlyphID glyphID);

  bool generatePath(GlyphID glyphID, Path* path);

  std::shared_ptr<TextureBuffer> generateImage(GlyphID glyphId, Matrix* matrix);

 private:
  FTScalerContext(std::shared_ptr<Typeface> typeFace, FTScalerContextRec rec);

  bool valid() const {
    return _face != nullptr && ftSize != nullptr;
  }

  int setupSize();

  bool getCBoxForLetter(char letter, FT_BBox* bbox);

  void emboldenIfNeeded(FT_Face face, FT_GlyphSlot glyph, GlyphID gid) const;

  void getBBoxForCurrentGlyph(FT_BBox* bbox);

  std::shared_ptr<Typeface> typeface;
  FTScalerContextRec rec;
  FTFace* _face = nullptr;
  FT_Size ftSize = nullptr;
  FT_Int strikeIndex = -1;  // The bitmap strike for the face (or -1 if none).
  Matrix matrix22Scalar = Matrix::I();
  FT_Matrix matrix22 = {};
  Point scale = Point::Make(1.f, 1.f);
  FT_Int32 loadGlyphFlags = 0;
};
}  // namespace tgfx
