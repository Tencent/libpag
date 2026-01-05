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

#include "ShapeGroup.h"
#include "codec/tags/ShapeTag.h"

namespace pag {
void ReadTagsOfShapeGroup(DecodeStream* stream, TagCode code,
                          std::vector<ShapeElement*>* contents) {
  ReadShape(stream, code, contents);
}

void ReadShapeGroupElements(DecodeStream* stream, void* target) {
  auto shape = reinterpret_cast<ShapeGroupElement*>(target);
  ReadTags(stream, &(shape->elements), ReadTagsOfShapeGroup);
}

bool WriteShapeGroupElements(EncodeStream* stream, void* target) {
  auto shape = reinterpret_cast<ShapeGroupElement*>(target);
  if (!shape->elements.empty()) {
    WriteShape(stream, &(shape->elements));
    WriteEndTag(stream);
  }
  return !shape->elements.empty();
}

std::unique_ptr<BlockConfig> ShapeGroupTag(ShapeGroupElement* shape) {
  if (shape->transform == nullptr) {
    shape->transform = new ShapeTransform();
  }
  auto transform = shape->transform;
  auto tagConfig = new BlockConfig(TagCode::ShapeGroup);
  AddAttribute(tagConfig, &shape->blendMode, AttributeType::Value,
               static_cast<uint8_t>(BlendMode::Normal));
  AddAttribute(tagConfig, &transform->anchorPoint, AttributeType::SpatialProperty, Point::Zero());
  AddAttribute(tagConfig, &transform->position, AttributeType::SpatialProperty, Point::Zero());
  AddAttribute(tagConfig, &transform->scale, AttributeType::MultiDimensionProperty,
               Point::Make(1, 1));
  AddAttribute(tagConfig, &transform->skew, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &transform->skewAxis, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &transform->rotation, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &transform->opacity, AttributeType::SimpleProperty, Opaque);
  AddCustomAttribute(tagConfig, shape, ReadShapeGroupElements, WriteShapeGroupElements);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
