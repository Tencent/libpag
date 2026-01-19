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

#include "Gradient.h"
#include "Dashes.h"

namespace pag {
static void ReadStrokeDashes(DecodeStream* stream, void* target) {
  auto shape = reinterpret_cast<GradientStrokeElement*>(target);
  shape->dashOffset = ReadDashes(stream, shape->dashes);
}

static bool WriteStrokeDashes(EncodeStream* stream, void* target) {
  auto shape = reinterpret_cast<GradientStrokeElement*>(target);
  WriteDashes(stream, shape->dashes, shape->dashOffset);
  return !shape->dashes.empty();
}

template <typename T>
void AddGradientCommonTags(BlockConfig* tagConfig, T* shape) {
  AddAttribute(tagConfig, &shape->fillType, AttributeType::Value,
               static_cast<uint8_t>(GradientFillType::Linear));
  AddAttribute(tagConfig, &shape->startPoint, AttributeType::SpatialProperty, Point::Zero());
  AddAttribute(tagConfig, &shape->endPoint, AttributeType::SpatialProperty, Point::Make(100, 0));
  AddAttribute(tagConfig, &shape->colors, AttributeType::SimpleProperty,
               GradientColorHandle(new GradientColor()));
  AddAttribute(tagConfig, &shape->opacity, AttributeType::SimpleProperty, Opaque);
}

std::unique_ptr<BlockConfig> GradientStrokeTag(GradientStrokeElement* shape) {
  auto tagConfig = new BlockConfig(TagCode::GradientStroke);
  AddAttribute(tagConfig, &shape->blendMode, AttributeType::Value,
               static_cast<uint8_t>(BlendMode::Normal));
  AddAttribute(tagConfig, &shape->composite, AttributeType::Value,
               static_cast<uint8_t>(CompositeOrder::BelowPreviousInSameGroup));
  AddGradientCommonTags(tagConfig, shape);
  AddAttribute(tagConfig, &shape->strokeWidth, AttributeType::SimpleProperty, 2.0f);
  AddAttribute(tagConfig, &shape->lineCap, AttributeType::Value,
               static_cast<uint8_t>(LineCap::Butt));
  AddAttribute(tagConfig, &shape->lineJoin, AttributeType::Value,
               static_cast<uint8_t>(LineJoin::Miter));
  AddAttribute(tagConfig, &shape->miterLimit, AttributeType::SimpleProperty, 4.0f);
  AddCustomAttribute(tagConfig, shape, ReadStrokeDashes, WriteStrokeDashes);
  return std::unique_ptr<BlockConfig>(tagConfig);
}

std::unique_ptr<BlockConfig> GradientFillTag(GradientFillElement* shape) {
  auto tagConfig = new BlockConfig(TagCode::GradientFill);
  AddAttribute(tagConfig, &shape->blendMode, AttributeType::Value,
               static_cast<uint8_t>(BlendMode::Normal));
  AddAttribute(tagConfig, &shape->composite, AttributeType::Value,
               static_cast<uint8_t>(CompositeOrder::BelowPreviousInSameGroup));
  AddAttribute(tagConfig, &shape->fillRule, AttributeType::Value,
               static_cast<uint8_t>(FillRule::NonZeroWinding));
  AddGradientCommonTags(tagConfig, shape);
  return std::unique_ptr<BlockConfig>(tagConfig);
}

}  // namespace pag
