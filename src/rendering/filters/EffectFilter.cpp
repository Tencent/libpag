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

#include "EffectFilter.h"
#include <tgfx/core/Canvas.h>
#include "BrightnessContrastFilter.h"
#include "BulgeFilter.h"
#include "CornerPinRuntimeFilter.h"
#include "DisplacementMapFilter.h"
#include "HueSaturationFilter.h"
#include "LevelsIndividualFilter.h"
#include "MosaicFilter.h"
#include "MotionTileFilter.h"
#include "RadialBlurFilter.h"
#include "layerstyle/StrokeFilter.h"
#include "rendering/filters/gaussianblur/GaussianBlurFilter.h"
#include "rendering/filters/glow/GlowFilter.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {

std::vector<tgfx::Point> ComputeVerticesForMotionBlurAndBulge(const tgfx::Rect& inputBounds,
                                                              const tgfx::Rect& outputBounds) {
  std::vector<tgfx::Point> vertices = {};
  tgfx::Point contentPoint[4] = {{outputBounds.left, outputBounds.bottom},
                                 {outputBounds.right, outputBounds.bottom},
                                 {outputBounds.left, outputBounds.top},
                                 {outputBounds.right, outputBounds.top}};
  auto deltaX = outputBounds.left - inputBounds.left;
  auto deltaY = outputBounds.top - inputBounds.top;
  tgfx::Point texturePoints[4] = {
      {deltaX, (outputBounds.height() + deltaY)},
      {(outputBounds.width() + deltaX), (outputBounds.height() + deltaY)},
      {deltaX, deltaY},
      {(outputBounds.width() + deltaX), deltaY}};
  for (int ii = 0; ii < 4; ii++) {
    vertices.push_back(contentPoint[ii]);
    vertices.push_back(texturePoints[ii]);
  }
  return vertices;
}

std::shared_ptr<EffectFilter> EffectFilter::Make(Effect* effect) {
  EffectFilter* filter = nullptr;
  switch (effect->type()) {
    case EffectType::CornerPin:
      filter = new CornerPinFilter(effect);
      break;
    case EffectType::Bulge:
      filter = new BulgeFilter(effect);
      break;
    case EffectType::MotionTile:
      filter = new MotionTileFilter(effect);
      break;
    case EffectType::Glow:
      filter = new GlowFilter(effect);
      break;
    case EffectType::LevelsIndividual:
      filter = new LevelsIndividualFilter(effect);
      break;
    case EffectType::FastBlur:
      filter = new GaussianBlurFilter(effect);
      break;
    case EffectType::DisplacementMap:
      filter = new DisplacementMapFilter(effect);
      break;
    case EffectType::RadialBlur:
      filter = new RadialBlurFilter(effect);
      break;
    case EffectType::Mosaic:
      filter = new MosaicFilter(effect);
      break;
    case EffectType::BrightnessContrast:
      filter = new BrightnessContrastFilter(effect);
      break;
    case EffectType::HueSaturation:
      filter = new HueSaturationFilter(effect);
      break;
    default:
      break;
  }
  return std::shared_ptr<EffectFilter>(filter);
}

void EffectFilter::applyFilter(tgfx::Canvas* canvas, std::shared_ptr<tgfx::Image> source) {
  auto filter = tgfx::ImageFilter::Runtime(createRuntimeEffect());
  if (!filter) {
    return;
  }
  tgfx::Paint paint;
  paint.setImageFilter(filter);
  canvas->drawImage(source, &paint);
}

std::shared_ptr<tgfx::RuntimeEffect> EffectFilter::createRuntimeEffect() {
  return nullptr;
}

}  // namespace pag
