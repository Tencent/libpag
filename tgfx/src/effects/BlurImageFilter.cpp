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

#include "BlurImageFilter.h"
#include "gpu/DualBlurFragmentProcessor.h"
#include "gpu/SurfaceDrawContext.h"
#include "gpu/TextureSampler.h"
#include "gpu/TiledTextureEffect.h"

namespace tgfx {
static const float BLUR_LEVEL_1_LIMIT = 10.0f;
static const float BLUR_LEVEL_2_LIMIT = 15.0f;
static const float BLUR_LEVEL_3_LIMIT = 55.0f;
static const float BLUR_LEVEL_4_LIMIT = 120.0f;
static const float BLUR_LEVEL_5_LIMIT = 300.0f;

static const float BLUR_LEVEL_MAX_LIMIT = BLUR_LEVEL_5_LIMIT;

static const int BLUR_LEVEL_1_DEPTH = 1;
static const int BLUR_LEVEL_2_DEPTH = 2;
static const int BLUR_LEVEL_3_DEPTH = 2;
static const int BLUR_LEVEL_4_DEPTH = 3;
static const int BLUR_LEVEL_5_DEPTH = 3;

static const float BLUR_LEVEL_1_SCALE = 1.0f;
static const float BLUR_LEVEL_2_SCALE = 0.8f;
static const float BLUR_LEVEL_3_SCALE = 0.5f;
static const float BLUR_LEVEL_4_SCALE = 0.5f;
static const float BLUR_LEVEL_5_SCALE = 0.5f;

static const float BLUR_STABLE = 10.0f;

static std::tuple<int, float, float> Get(float blurriness) {
  blurriness = blurriness < BLUR_LEVEL_MAX_LIMIT ? blurriness : BLUR_LEVEL_MAX_LIMIT;
  if (blurriness < BLUR_LEVEL_1_LIMIT) {
    return {BLUR_LEVEL_1_DEPTH, BLUR_LEVEL_1_SCALE, blurriness / BLUR_LEVEL_1_LIMIT * 2.0};
  } else if (blurriness < BLUR_LEVEL_2_LIMIT) {
    return {BLUR_LEVEL_2_DEPTH, BLUR_LEVEL_2_SCALE,
            (blurriness - BLUR_STABLE) / (BLUR_LEVEL_2_LIMIT - BLUR_STABLE) * 3.0};
  } else if (blurriness < BLUR_LEVEL_3_LIMIT) {
    return {BLUR_LEVEL_3_DEPTH, BLUR_LEVEL_3_SCALE,
            (blurriness - BLUR_STABLE) / (BLUR_LEVEL_3_LIMIT - BLUR_STABLE) * 5.0};
  } else if (blurriness < BLUR_LEVEL_4_LIMIT) {
    return {BLUR_LEVEL_4_DEPTH, BLUR_LEVEL_4_SCALE,
            (blurriness - BLUR_STABLE) / (BLUR_LEVEL_4_LIMIT - BLUR_STABLE) * 6.0};
  } else {
    return {
        BLUR_LEVEL_5_DEPTH, BLUR_LEVEL_5_SCALE,
        6.0 + (blurriness - BLUR_STABLE * 12.0) / (BLUR_LEVEL_5_LIMIT - BLUR_STABLE * 12.0) * 5.0};
  }
}

std::shared_ptr<ImageFilter> ImageFilter::Blur(float blurrinessX, float blurrinessY,
                                               TileMode tileMode, const Rect& cropRect) {
  if (blurrinessX < 0 || blurrinessY < 0 || (blurrinessX == 0 && blurrinessY == 0)) {
    return nullptr;
  }
  auto x = Get(blurrinessX);
  auto y = Get(blurrinessY);
  return std::make_shared<BlurImageFilter>(
      Point::Make(std::get<2>(x), std::get<2>(y)), std::max(std::get<1>(x), std::get<1>(y)),
      std::max(std::get<0>(x), std::get<0>(y)), tileMode, cropRect);
}

void BlurImageFilter::draw(std::shared_ptr<Image> image, Surface* toSurface, bool isDown) {
  auto drawContext = std::make_unique<SurfaceDrawContext>(toSurface);
  auto dstRect = Rect::MakeWH(toSurface->width(), toSurface->height());
  auto localMatrix = Matrix::MakeScale(static_cast<float>(image->width()) / dstRect.width(),
                                       static_cast<float>(image->height()) / dstRect.height());
  auto texelSize = Point::Make(0.5f / static_cast<float>(image->width()),
                               0.5f / static_cast<float>(image->height()));
  auto processor =
      image->asFragmentProcessor(toSurface->getContext(), toSurface->options()->flags(), tileMode,
                                 tileMode, SamplingOptions());
  drawContext->fillRectWithFP(
      dstRect, localMatrix,
      DualBlurFragmentProcessor::Make(isDown ? DualBlurPassMode::Down : DualBlurPassMode::Up,
                                      std::move(processor), blurOffset, texelSize));
}

static std::shared_ptr<Image> ExtendImage(Context* context, std::shared_ptr<Image> image,
                                          const Rect& dstBounds, TileMode tileMode) {
  auto dstWidth = dstBounds.width();
  auto dstHeight = dstBounds.height();
  auto surface = Surface::Make(context, static_cast<int>(dstWidth), static_cast<int>(dstHeight));
  if (surface == nullptr) {
    return nullptr;
  }
  auto canvas = surface->getCanvas();
  auto localMatrix = Matrix::MakeTrans(-dstBounds.left, -dstBounds.top);
  canvas->concat(localMatrix);
  auto shader = Shader::MakeImageShader(image, tileMode, tileMode);
  Paint paint = {};
  paint.setShader(shader);
  canvas->drawRect(dstBounds, paint);
  return surface->makeImageSnapshot();
}

std::pair<std::shared_ptr<Image>, Point> BlurImageFilter::filterImage(
    const ImageFilterContext& context) {
  auto image = context.source;
  if (image == nullptr) {
    return {};
  }
  auto inputBounds = Rect::MakeWH(image->width(), image->height());
  Rect dstBounds = Rect::MakeEmpty();
  if (!applyCropRect(inputBounds, &dstBounds, &context.clipBounds)) {
    return {};
  }
  if (!inputBounds.intersect(dstBounds)) {
    return {};
  }
  dstBounds.roundOut();
  std::shared_ptr<Image> last;
  if (dstBounds != Rect::MakeWH(image->width(), image->height())) {
    last = ExtendImage(context.context, image, dstBounds, tileMode);
    if (last == nullptr) {
      return {};
    }
    image = last;
  }
  auto dstOffset = Point::Make(dstBounds.x(), dstBounds.y());
  int tw = static_cast<int>(dstBounds.width() * downScaling);
  int th = static_cast<int>(dstBounds.height() * downScaling);
  std::vector<std::pair<int, int>> upSurfaces;
  upSurfaces.emplace_back(static_cast<int>(dstBounds.width()),
                          static_cast<int>(dstBounds.height()));
  for (int i = 0; i < iteration; ++i) {
    auto surface = Surface::Make(context.context, tw, th);
    if (surface == nullptr) {
      return {};
    }
    upSurfaces.emplace_back(tw, th);
    draw(image, surface.get(), true);
    last = surface->makeImageSnapshot();
    image = last;
    tw = std::max(static_cast<int>(static_cast<float>(tw) * downScaling), 1);
    th = std::max(static_cast<int>(static_cast<float>(th) * downScaling), 1);
  }
  for (int i = static_cast<int>(upSurfaces.size()) - 2; i >= 0; --i) {
    auto width = upSurfaces[i].first;
    auto height = upSurfaces[i].second;
    auto surface = Surface::Make(context.context, width, height);
    if (surface == nullptr) {
      return {};
    }
    draw(last, surface.get(), false);
    last = surface->makeImageSnapshot();
  }
  return {last, dstOffset};
}

Rect BlurImageFilter::onFilterNodeBounds(const Rect& srcRect) const {
  auto mul = static_cast<float>(std::pow(2, iteration)) / downScaling;
  return srcRect.makeOutset(blurOffset.x * mul, blurOffset.y * mul);
}
}  // namespace tgfx
