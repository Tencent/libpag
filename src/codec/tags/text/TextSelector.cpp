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

#include "TextSelector.h"

namespace pag {
std::unique_ptr<BlockConfig> TextRangeSelectorTag(TextRangeSelector* selector) {
  auto tagConfig = new BlockConfig(TagCode::TextRangeSelector);

  AddAttribute(tagConfig, &selector->start, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &selector->end, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &selector->offset, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &selector->units, AttributeType::Value,
               static_cast<uint8_t>(TextRangeSelectorUnits::Percentage));
  AddAttribute(tagConfig, &selector->basedOn, AttributeType::Value,
               static_cast<uint8_t>(TextSelectorBasedOn::Characters));
  AddAttribute(tagConfig, &selector->mode, AttributeType::DiscreteProperty,
               static_cast<uint8_t>(TextSelectorMode::Add));
  AddAttribute(tagConfig, &selector->amount, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &selector->shape, AttributeType::Value,
               static_cast<uint8_t>(TextRangeSelectorShape::Square));
  AddAttribute(tagConfig, &selector->smoothness, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &selector->easeHigh, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &selector->easeLow, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &selector->randomizeOrder, AttributeType::BitFlag, false);
  AddAttribute(tagConfig, &selector->randomSeed, AttributeType::SimpleProperty,
               static_cast<uint16_t>(0));

  return std::unique_ptr<BlockConfig>(tagConfig);
}

std::unique_ptr<BlockConfig> TextWigglySelectorTag(TextWigglySelector* selector) {
  auto tagConfig = new BlockConfig(TagCode::TextWigglySelector);

  AddAttribute(tagConfig, &selector->mode, AttributeType::DiscreteProperty,
               static_cast<uint8_t>(TextSelectorMode::Intersect));
  AddAttribute(tagConfig, &selector->maxAmount, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &selector->minAmount, AttributeType::SimpleProperty, -1.0f);
  AddAttribute(tagConfig, &selector->basedOn, AttributeType::Value,
               static_cast<uint8_t>(TextSelectorBasedOn::Characters));
  AddAttribute(tagConfig, &selector->wigglesPerSecond, AttributeType::SimpleProperty, 2.0f);
  AddAttribute(tagConfig, &selector->correlation, AttributeType::SimpleProperty, 0.5f);
  AddAttribute(tagConfig, &selector->temporalPhase, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &selector->spatialPhase, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &selector->lockDimensions, AttributeType::DiscreteProperty, false);
  AddAttribute(tagConfig, &selector->randomSeed, AttributeType::SimpleProperty,
               static_cast<uint16_t>(0));

  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
