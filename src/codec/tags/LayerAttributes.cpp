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

#include "LayerAttributes.h"

namespace pag {
std::unique_ptr<BlockConfig> MakeFromLayerAttributesTag(TagCode tagCode, Layer* layer) {
  auto tagConfig = new BlockConfig(tagCode);
  AddAttribute(tagConfig, &layer->isActive, AttributeType::BitFlag, true);
  AddAttribute(tagConfig, &layer->autoOrientation, AttributeType::BitFlag, false);
  if (tagConfig->tagCode == TagCode::LayerAttributesV3) {
    if (layer->type() != LayerType::Camera) {
      AddAttribute(tagConfig, &layer->motionBlur, AttributeType::BitFlag, false);
    }
  }
  AddAttribute(tagConfig, &layer->parent, AttributeType::Value, static_cast<Layer*>(nullptr));
  AddAttribute(tagConfig, &layer->stretch, AttributeType::Value, DefaultRatio);
  AddAttribute(tagConfig, &layer->startTime, AttributeType::Value, ZeroFrame);
  if (layer->type() != LayerType::Camera) {
    AddAttribute(tagConfig, &layer->blendMode, AttributeType::Value,
                 static_cast<uint8_t>(BlendMode::Normal));
    AddAttribute(tagConfig, &layer->trackMatteType, AttributeType::Value,
                 static_cast<uint8_t>(TrackMatteType::None));
  }
  AddAttribute(tagConfig, &layer->timeRemap, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &layer->duration, AttributeType::FixedValue, ZeroFrame);
  if (tagConfig->tagCode == TagCode::LayerAttributesV2 ||
      tagConfig->tagCode == TagCode::LayerAttributesV3) {
    AddAttribute(tagConfig, &layer->name, AttributeType::Value, std::string(""));
  }
  return std::unique_ptr<BlockConfig>(tagConfig);
}

std::unique_ptr<BlockConfig> LayerAttributesTag(Layer* layer) {
  return MakeFromLayerAttributesTag(TagCode::LayerAttributes, layer);
}

std::unique_ptr<BlockConfig> LayerAttributesTagV2(Layer* layer) {
  return MakeFromLayerAttributesTag(TagCode::LayerAttributesV2, layer);
}

std::unique_ptr<BlockConfig> LayerAttributesTagV3(Layer* layer) {
  return MakeFromLayerAttributesTag(TagCode::LayerAttributesV3, layer);
}

}  // namespace pag
