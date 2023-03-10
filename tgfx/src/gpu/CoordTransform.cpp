/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "CoordTransform.h"
#include "gpu/TextureSampler.h"

namespace tgfx {
Matrix CoordTransform::getTotalMatrix() const {
  if (texture == nullptr) {
    return matrix;
  }
  auto combined = matrix;
  // normalize
  auto scale = texture->getTextureCoord(1, 1);
  combined.postScale(scale.x, scale.y);
  // If the texture has a crop rectangle, we need to shrink it to prevent bilinear sampling beyond
  // the edge of the crop rectangle.
  auto textureType = texture->getSampler()->type();
  auto edgePoint = texture->getTextureCoord(texture->width(), texture->height());
  static constexpr Point FullEdge = Point::Make(1.0f, 1.0f);
  if (textureType != TextureType::Rectangle && edgePoint != FullEdge) {
    // https://cs.android.com/android/platform/superproject/+/master:frameworks/native/libs/nativedisplay/surfacetexture/SurfaceTexture.cpp;l=275;drc=master;bpv=0;bpt=1
    // https://stackoverflow.com/questions/6023400/opengl-es-texture-coordinates-slightly-off
    // Normally this would just need to take 1/2 a texel off each end, but because the chroma
    // channels of YUV420 images are subsampled we may need to shrink the crop region by a whole
    // texel on each side.
    auto shrinkAmount = textureType == TextureType::External ? 1.0f : 0.5f;
    float scaleFactorX = 2.0f;
    float scaleFactorY = 2.0f;
    // If we have an RGBAAA layout, double the scaleFactor on the direction of the alpha area
    // exists.
    if (alphaStart.x > 0) {
      scaleFactorX *= 2;
    } else if (alphaStart.y > 0) {
      scaleFactorY *= 2;
    }
    scale = texture->getTextureCoord(
        static_cast<float>(texture->width()) - (scaleFactorX * shrinkAmount),
        static_cast<float>(texture->height()) - (scaleFactorY * shrinkAmount));
    combined.postScale(scale.x, scale.y);
    auto translate = texture->getTextureCoord(shrinkAmount, shrinkAmount);
    combined.postTranslate(translate.x, translate.y);
  }
  if (texture->origin() == ImageOrigin::BottomLeft) {
    combined.postScale(1, -1);
    auto translate = texture->getTextureCoord(0, static_cast<float>(texture->height()));
    combined.postTranslate(translate.x, translate.y);
  }
  return combined;
}
}  // namespace tgfx
