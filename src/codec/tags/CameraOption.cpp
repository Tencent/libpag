/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "CameraOption.h"

namespace pag {

std::unique_ptr<BlockConfig> CameraOptionTag(CameraOption* cameraOption) {
  auto tagConfig = new BlockConfig(TagCode::CameraOption);
  AddAttribute(tagConfig, &cameraOption->zoom, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &cameraOption->depthOfField, AttributeType::DiscreteProperty, false);
  AddAttribute(tagConfig, &cameraOption->focusDistance, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &cameraOption->aperture, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &cameraOption->blurLevel, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &cameraOption->irisShape, AttributeType::DiscreteProperty,
               static_cast<uint8_t>(IrisShapeType::FastRectangle));
  AddAttribute(tagConfig, &cameraOption->irisRotation, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &cameraOption->irisRoundness, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &cameraOption->irisAspectRatio, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &cameraOption->irisDiffractionFringe, AttributeType::SimpleProperty,
               0.0f);
  AddAttribute(tagConfig, &cameraOption->highlightGain, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &cameraOption->highlightThreshold, AttributeType::SimpleProperty, 255.0f);
  AddAttribute(tagConfig, &cameraOption->highlightSaturation, AttributeType::SimpleProperty, 0.0f);
  return std::unique_ptr<BlockConfig>(tagConfig);
}

}  // namespace pag
