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
#include "tgfx/core/Image.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/TileMode.h"
#include "tgfx/gpu/Context.h"

namespace tgfx {
struct ImageFilterContext {
  ImageFilterContext(Context* context, Matrix matrix, Rect clipBounds, std::shared_ptr<Image> image)
      : context(context), deviceMatrix(matrix), clipBounds(clipBounds), source(std::move(image)) {
  }

  Context* context = nullptr;
  Matrix deviceMatrix = Matrix::I();
  Rect clipBounds = Rect::MakeEmpty();
  std::shared_ptr<Image> source;
};

class ImageFilter {
 public:
  static std::shared_ptr<ImageFilter> Blur(float blurrinessX, float blurrinessY,
                                           TileMode tileMode = TileMode::Decal,
                                           const Rect& cropRect = Rect::MakeEmpty());

  static std::shared_ptr<ImageFilter> DropShadow(float dx, float dy, float blurrinessX,
                                                 float blurrinessY, const Color& color,
                                                 const Rect& cropRect = Rect::MakeEmpty());

  static std::shared_ptr<ImageFilter> DropShadowOnly(float dx, float dy, float blurrinessX,
                                                     float blurrinessY, const Color& color,
                                                     const Rect& cropRect = Rect::MakeEmpty());

  virtual ~ImageFilter() = default;

  virtual std::pair<std::shared_ptr<Image>, Point> filterImage(
      const ImageFilterContext& context) = 0;

  Rect filterBounds(const Rect& rect) const;

 protected:
  explicit ImageFilter(const Rect cropRect) : cropRect(cropRect) {
  }

  bool applyCropRect(const Rect& srcRect, Rect* dstRect, const Rect* clipRect = nullptr) const;

  virtual Rect onFilterNodeBounds(const Rect& srcRect) const;

  Rect cropRect;
};
}  // namespace tgfx
