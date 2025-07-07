/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "ImageFillRule.h"
#include "codec/Attributes.h"

namespace pag {

static std::unique_ptr<BlockConfig> ImageFillRuleTag(ImageFillRule* imageFillRule, TagCode code) {
  auto tagConfig = new BlockConfig(code);
  AddAttribute(tagConfig, &imageFillRule->scaleMode, AttributeType::Value,
               static_cast<uint8_t>(PAGScaleMode::LetterBox));
  AddAttribute(tagConfig, &imageFillRule->timeRemap, AttributeType::SimpleProperty,
               static_cast<Frame>(0));
  return std::unique_ptr<BlockConfig>(tagConfig);
}

static std::unique_ptr<BlockConfig> ImageFillRuleTagV1(ImageFillRule* imageFillRule) {
  return ImageFillRuleTag(imageFillRule, TagCode::ImageFillRule);
}

static std::unique_ptr<BlockConfig> ImageFillRuleTagV2(ImageFillRule* imageFillRule) {
  return ImageFillRuleTag(imageFillRule, TagCode::ImageFillRuleV2);
}

void ReadImageFillRule(pag::DecodeStream* stream, pag::ImageLayer* layer, TagCode code) {
  layer->imageFillRule = new ImageFillRule();

  if (code == TagCode::ImageFillRule) {
    if (ReadTagBlock(stream, layer->imageFillRule, ImageFillRuleTagV1)) {
      // The interpolation type should be linear in v1 tag, but we store it as hold, so we need to fix
      // it here.
      auto timeRemap = layer->imageFillRule->timeRemap;
      if (timeRemap != nullptr && timeRemap->animatable()) {
        auto AnimatableTimeRemap = static_cast<AnimatableProperty<Frame>*>(timeRemap);
        for (auto& keyFrame : AnimatableTimeRemap->keyframes) {
          keyFrame->interpolationType = pag::KeyframeInterpolationType::Linear;
        }
      }
    }
  } else {
    ReadTagBlock(stream, layer->imageFillRule, ImageFillRuleTagV2);
  }
}

void WriteImageFillRule(pag::EncodeStream* stream, pag::ImageFillRule* imageFillRule) {
  auto timeRemap = imageFillRule->timeRemap;
  if (timeRemap != nullptr && timeRemap->animatable()) {
    for (auto& keyFrame : static_cast<AnimatableProperty<Frame>*>(timeRemap)->keyframes) {
      if (keyFrame->interpolationType != pag::KeyframeInterpolationType::Linear) {
        WriteTagBlock(stream, imageFillRule, ImageFillRuleTagV2);
        return;
      }
    }
  }

  WriteTagBlock(stream, imageFillRule, ImageFillRuleTagV1);
}
}  // namespace pag
