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

#include "Stroke.h"
#include "Dashes.h"

namespace pag {
static void ReadStrokeDashes(DecodeStream* stream, void* target) {
  auto shape = reinterpret_cast<StrokeElement*>(target);
  shape->dashOffset = ReadDashes(stream, shape->dashes);
}

static bool WriteStrokeDashes(EncodeStream* stream, void* target) {
  auto shape = reinterpret_cast<StrokeElement*>(target);
  WriteDashes(stream, shape->dashes, shape->dashOffset);
  return !shape->dashes.empty();
}

std::unique_ptr<BlockConfig> StrokeTag(StrokeElement* shape) {
  auto tagConfig = new BlockConfig(TagCode::Stroke);
  AddAttribute(tagConfig, &shape->blendMode, AttributeType::Value,
               static_cast<uint8_t>(BlendMode::Normal));
  AddAttribute(tagConfig, &shape->composite, AttributeType::Value,
               static_cast<uint8_t>(CompositeOrder::BelowPreviousInSameGroup));
  AddAttribute(tagConfig, &shape->lineCap, AttributeType::Value,
               static_cast<uint8_t>(LineCap::Butt));
  AddAttribute(tagConfig, &shape->lineJoin, AttributeType::Value,
               static_cast<uint8_t>(LineJoin::Miter));
  AddAttribute(tagConfig, &shape->miterLimit, AttributeType::SimpleProperty, 4.0f);
  AddAttribute(tagConfig, &shape->color, AttributeType::SimpleProperty, White);
  AddAttribute(tagConfig, &shape->opacity, AttributeType::SimpleProperty, Opaque);
  AddAttribute(tagConfig, &shape->strokeWidth, AttributeType::SimpleProperty, 2.0f);
  AddCustomAttribute(tagConfig, shape, ReadStrokeDashes, WriteStrokeDashes);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
