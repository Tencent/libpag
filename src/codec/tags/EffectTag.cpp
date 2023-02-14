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

#include "EffectTag.h"
#include "effects/BrightnessContrastEffect.h"
#include "effects/BulgeEffect.h"
#include "effects/CornerPinEffect.h"
#include "effects/DisplacementMapEffect.h"
#include "effects/FastBlurEffect.h"
#include "effects/GlowEffect.h"
#include "effects/LevelsIndividualEffect.h"
#include "effects/MosaicEffect.h"
#include "effects/MotionTileEffect.h"
#include "effects/RadialBlurEffect.h"

namespace pag {
bool ReadEffect(DecodeStream* stream, TagCode code, Layer* layer) {
  Effect* effect = nullptr;
  switch (code) {
    case TagCode::MotionTileEffect:
      effect = ReadTagBlock(stream, MotionTileEffectTag);
      break;
    case TagCode::LevelsIndividualEffect:
      effect = ReadTagBlock(stream, LevelsIndividualEffectTag);
      break;
    case TagCode::CornerPinEffect:
      effect = ReadTagBlock(stream, CornerPinEffectTag);
      break;
    case TagCode::BulgeEffect:
      effect = ReadTagBlock(stream, BulgeEffectTag);
      break;
    case TagCode::FastBlurEffect:
      effect = ReadTagBlock(stream, FastBlurEffectTag);
      break;
    case TagCode::GlowEffect:
      effect = ReadTagBlock(stream, GlowEffectTag);
      break;
    case TagCode::DisplacementMapEffect:
      effect = ReadTagBlock(stream, DisplacementMapEffectTag);
      break;
    case TagCode::RadialBlurEffect:
      effect = ReadTagBlock(stream, RadialBlurEffectTag);
      break;
    case TagCode::MosaicEffect:
      effect = ReadTagBlock(stream, MosaicEffectTag);
      break;
    case TagCode::BrightnessContrastEffect:
      effect = ReadTagBlock(stream, BrightnessContrastEffectTag);
      break;
    default:
      break;
  }
  if (effect) {
    layer->effects.push_back(effect);
  }
  return effect != nullptr;
}

void WriteEffects(EncodeStream* stream, const std::vector<Effect*>& effects) {
  for (auto& effect : effects) {
    switch (effect->type()) {
      case EffectType::MotionTile:
        WriteTagBlock(stream, static_cast<MotionTileEffect*>(effect), MotionTileEffectTag);
        break;
      case EffectType::LevelsIndividual:
        WriteTagBlock(stream, static_cast<LevelsIndividualEffect*>(effect),
                      LevelsIndividualEffectTag);
        break;
      case EffectType::CornerPin:
        WriteTagBlock(stream, static_cast<CornerPinEffect*>(effect), CornerPinEffectTag);
        break;
      case EffectType::Bulge:
        WriteTagBlock(stream, static_cast<BulgeEffect*>(effect), BulgeEffectTag);
        break;
      case EffectType::FastBlur:
        WriteTagBlock(stream, static_cast<FastBlurEffect*>(effect), FastBlurEffectTag);
        break;
      case EffectType::Glow:
        WriteTagBlock(stream, static_cast<GlowEffect*>(effect), GlowEffectTag);
        break;
      case EffectType::DisplacementMap:
        WriteTagBlock(stream, static_cast<DisplacementMapEffect*>(effect),
                      DisplacementMapEffectTag);
        break;
      case EffectType::RadialBlur:
        WriteTagBlock(stream, static_cast<RadialBlurEffect*>(effect), RadialBlurEffectTag);
        break;
      case EffectType::Mosaic:
        WriteTagBlock(stream, static_cast<MosaicEffect*>(effect), MosaicEffectTag);
        break;
      case EffectType::BrightnessContrast:
        WriteTagBlock(stream, static_cast<BrightnessContrastEffect*>(effect), BrightnessContrastEffectTag);
        break;
      default:
        break;
    }
  }
}
}  // namespace pag
