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

#include "TextAnimatorTag.h"
#include "TextSelector.h"
#include "base/utils/EnumClassHash.h"

namespace pag {

static std::unique_ptr<BlockConfig> TextAnimatorTrackingTypeTag(
    TextAnimatorTypographyProperties* properties) {
  auto tagConfig = new BlockConfig(TagCode::TextAnimatorPropertiesTrackingType);
  AddAttribute(tagConfig, &properties->trackingType, AttributeType::DiscreteProperty,
               static_cast<uint8_t>(TextAnimatorTrackingType::BeforeAndAfter));
  return std::unique_ptr<BlockConfig>(tagConfig);
}

static std::unique_ptr<BlockConfig> TextAnimatorTrackingAmountTag(
    TextAnimatorTypographyProperties* properties) {
  auto tagConfig = new BlockConfig(TagCode::TextAnimatorPropertiesTrackingAmount);
  AddAttribute(tagConfig, &properties->trackingAmount, AttributeType::SimpleProperty, 0.0f);
  return std::unique_ptr<BlockConfig>(tagConfig);
}

static std::unique_ptr<BlockConfig> TextAnimatorPositionTag(
    TextAnimatorTypographyProperties* properties) {
  auto tagConfig = new BlockConfig(TagCode::TextAnimatorPropertiesPosition);
  AddAttribute(tagConfig, &properties->position, AttributeType::SpatialProperty, Point::Zero());
  return std::unique_ptr<BlockConfig>(tagConfig);
}

static std::unique_ptr<BlockConfig> TextAnimatorScaleTag(
    TextAnimatorTypographyProperties* properties) {
  auto tagConfig = new BlockConfig(TagCode::TextAnimatorPropertiesScale);
  AddAttribute(tagConfig, &properties->scale, AttributeType::MultiDimensionProperty,
               Point::Make(1, 1));
  return std::unique_ptr<BlockConfig>(tagConfig);
}

static std::unique_ptr<BlockConfig> TextAnimatorRotationTag(
    TextAnimatorTypographyProperties* properties) {
  auto tagConfig = new BlockConfig(TagCode::TextAnimatorPropertiesRotation);
  AddAttribute(tagConfig, &properties->rotation, AttributeType::SimpleProperty, 0.0f);
  return std::unique_ptr<BlockConfig>(tagConfig);
}

static std::unique_ptr<BlockConfig> TextAnimatorOpacityTag(
    TextAnimatorTypographyProperties* properties) {
  auto tagConfig = new BlockConfig(TagCode::TextAnimatorPropertiesOpacity);
  AddAttribute(tagConfig, &properties->opacity, AttributeType::SimpleProperty, Opaque);
  return std::unique_ptr<BlockConfig>(tagConfig);
}

static std::unique_ptr<BlockConfig> TextAnimatorFillColorTag(
    TextAnimatorColorProperties* properties) {
  auto tagConfig = new BlockConfig(TagCode::TextAnimatorPropertiesFillColor);
  AddAttribute(tagConfig, &properties->fillColor, AttributeType::SimpleProperty, Red);
  return std::unique_ptr<BlockConfig>(tagConfig);
}

static std::unique_ptr<BlockConfig> TextAnimatorStrokeColorTag(
    TextAnimatorColorProperties* properties) {
  auto tagConfig = new BlockConfig(TagCode::TextAnimatorPropertiesStrokeColor);
  AddAttribute(tagConfig, &properties->strokeColor, AttributeType::SimpleProperty, Red);
  return std::unique_ptr<BlockConfig>(tagConfig);
}

static void CheckTypographyProperties(TextAnimator* animator) {
  if (animator->typographyProperties == nullptr) {
    animator->typographyProperties = new TextAnimatorTypographyProperties();
  }
}

static void CheckColorProperties(TextAnimator* animator) {
  if (animator->colorProperties == nullptr) {
    animator->colorProperties = new TextAnimatorColorProperties();
  }
}

static void ReadTag_TextRangeSelector(DecodeStream* stream, TextAnimator* animator) {
  auto selector = ReadTagBlock(stream, TextRangeSelectorTag);
  if (selector) {
    animator->selectors.push_back(selector);
  }
}

static void ReadTag_TextWigglySelector(DecodeStream* stream, TextAnimator* animator) {
  auto selector = ReadTagBlock(stream, TextWigglySelectorTag);
  if (selector) {
    animator->selectors.push_back(selector);
  }
}

static void ReadTag_TextAnimatorPropertiesTrackingType(DecodeStream* stream,
                                                       TextAnimator* animator) {
  CheckTypographyProperties(animator);
  ReadTagBlock(stream, animator->typographyProperties, TextAnimatorTrackingTypeTag);
}

static void ReadTag_TextAnimatorPropertiesTrackingAmount(DecodeStream* stream,
                                                         TextAnimator* animator) {
  CheckTypographyProperties(animator);
  ReadTagBlock(stream, animator->typographyProperties, TextAnimatorTrackingAmountTag);
}

static void ReadTag_TextAnimatorPropertiesPosition(DecodeStream* stream, TextAnimator* animator) {
  CheckTypographyProperties(animator);
  ReadTagBlock(stream, animator->typographyProperties, TextAnimatorPositionTag);
}

static void ReadTag_TextAnimatorPropertiesScale(DecodeStream* stream, TextAnimator* animator) {
  CheckTypographyProperties(animator);
  ReadTagBlock(stream, animator->typographyProperties, TextAnimatorScaleTag);
}

static void ReadTag_TextAnimatorPropertiesRotation(DecodeStream* stream, TextAnimator* animator) {
  CheckTypographyProperties(animator);
  ReadTagBlock(stream, animator->typographyProperties, TextAnimatorRotationTag);
}

static void ReadTag_TextAnimatorPropertiesOpacity(DecodeStream* stream, TextAnimator* animator) {
  CheckTypographyProperties(animator);
  ReadTagBlock(stream, animator->typographyProperties, TextAnimatorOpacityTag);
}

static void ReadTag_TextAnimatorPropertiesFillColor(DecodeStream* stream, TextAnimator* animator) {
  CheckColorProperties(animator);
  ReadTagBlock(stream, animator->colorProperties, TextAnimatorFillColorTag);
}

static void ReadTag_TextAnimatorPropertiesStrokeColor(DecodeStream* stream,
                                                      TextAnimator* animator) {
  CheckColorProperties(animator);
  ReadTagBlock(stream, animator->colorProperties, TextAnimatorStrokeColorTag);
}

using ReadTextAnimatorHandler = void(DecodeStream* stream, TextAnimator* animator);

static const std::unordered_map<TagCode, std::function<ReadTextAnimatorHandler>, EnumClassHash>
    elementHandlers = {
        {TagCode::TextRangeSelector, ReadTag_TextRangeSelector},
        {TagCode::TextWigglySelector, ReadTag_TextWigglySelector},
        {TagCode::TextAnimatorPropertiesTrackingType, ReadTag_TextAnimatorPropertiesTrackingType},
        {TagCode::TextAnimatorPropertiesTrackingAmount,
         ReadTag_TextAnimatorPropertiesTrackingAmount},
        {TagCode::TextAnimatorPropertiesPosition, ReadTag_TextAnimatorPropertiesPosition},
        {TagCode::TextAnimatorPropertiesScale, ReadTag_TextAnimatorPropertiesScale},
        {TagCode::TextAnimatorPropertiesRotation, ReadTag_TextAnimatorPropertiesRotation},
        {TagCode::TextAnimatorPropertiesOpacity, ReadTag_TextAnimatorPropertiesOpacity},
        {TagCode::TextAnimatorPropertiesFillColor, ReadTag_TextAnimatorPropertiesFillColor},
        {TagCode::TextAnimatorPropertiesStrokeColor, ReadTag_TextAnimatorPropertiesStrokeColor},
};

static void ReadTagsOfTextAnimator(DecodeStream* stream, TagCode code, TextAnimator* animator) {
  auto iter = elementHandlers.find(code);
  if (iter == elementHandlers.end()) {
    return;
  }
  iter->second(stream, animator);
}

void ReadTextAnimator(DecodeStream* stream, TextLayer* layer) {
  TextAnimator* animator = new TextAnimator();

  ReadTags(stream, animator, ReadTagsOfTextAnimator);

  layer->animators.push_back(animator);
}

static void WriteTextSelectors(EncodeStream* stream, const TextAnimator* animator) {
  for (auto& selector : animator->selectors) {
    auto type = static_cast<TextSelectorType>(selector->type());
    if (type == TextSelectorType::Range) {
      WriteTagBlock(stream, static_cast<TextRangeSelector*>(selector), TextRangeSelectorTag);
    } else if (type == TextSelectorType::Wiggly) {
      WriteTagBlock(stream, static_cast<TextWigglySelector*>(selector), TextWigglySelectorTag);
    }
  }
}

static void WriteColorProperties(EncodeStream* stream, const TextAnimator* animator) {
  if (animator->colorProperties != nullptr) {
    if (animator->colorProperties->fillColor != nullptr) {
      WriteTagBlock(stream, animator->colorProperties, TextAnimatorFillColorTag);
    }
    if (animator->colorProperties->strokeColor != nullptr) {
      WriteTagBlock(stream, animator->colorProperties, TextAnimatorStrokeColorTag);
    }
  }
}

static void WriteTypographyProperties(EncodeStream* stream, const TextAnimator* animator) {
  if (animator->typographyProperties != nullptr) {
    if (animator->typographyProperties->trackingType != nullptr) {
      WriteTagBlock(stream, animator->typographyProperties, TextAnimatorTrackingTypeTag);
    }
    if (animator->typographyProperties->trackingAmount != nullptr) {
      WriteTagBlock(stream, animator->typographyProperties, TextAnimatorTrackingAmountTag);
    }
    if (animator->typographyProperties->position != nullptr) {
      WriteTagBlock(stream, animator->typographyProperties, TextAnimatorPositionTag);
    }
    if (animator->typographyProperties->scale != nullptr) {
      WriteTagBlock(stream, animator->typographyProperties, TextAnimatorScaleTag);
    }
    if (animator->typographyProperties->rotation != nullptr) {
      WriteTagBlock(stream, animator->typographyProperties, TextAnimatorRotationTag);
    }
    if (animator->typographyProperties->opacity != nullptr) {
      WriteTagBlock(stream, animator->typographyProperties, TextAnimatorOpacityTag);
    }
  }
}

TagCode WriteTextAnimator(EncodeStream* stream, TextAnimator* animator) {
  WriteTextSelectors(stream, animator);
  WriteColorProperties(stream, animator);
  WriteTypographyProperties(stream, animator);
  WriteEndTag(stream);
  return TagCode::TextAnimator;
}
}  // namespace pag
