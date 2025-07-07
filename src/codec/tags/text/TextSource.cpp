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

#include "TextSource.h"

namespace pag {
std::unique_ptr<BlockConfig> TextSourceTag(TextLayer* layer) {
  auto tagConfig = new BlockConfig(TagCode::TextSource);
  AddAttribute(tagConfig, &layer->sourceText, AttributeType::DiscreteProperty,
               TextDocumentHandle(new TextDocument()));
  return std::unique_ptr<BlockConfig>(tagConfig);
}

std::unique_ptr<BlockConfig> TextSourceTagV2(TextLayer* layer) {
  auto tagConfig = new BlockConfig(TagCode::TextSourceV2);
  auto defaultTextDocument = TextDocumentHandle(new TextDocument());

  // This is the key property that triggers tag V2.
  defaultTextDocument->backgroundAlpha = 255;

  AddAttribute(tagConfig, &layer->sourceText, AttributeType::DiscreteProperty, defaultTextDocument);
  return std::unique_ptr<BlockConfig>(tagConfig);
}

std::unique_ptr<BlockConfig> TextSourceTagV3(TextLayer* layer) {
  auto tagConfig = new BlockConfig(TagCode::TextSourceV3);
  auto defaultTextDocument = TextDocumentHandle(new TextDocument());

  // This is the key property that triggers tag V3
  defaultTextDocument->direction = TextDirection::Horizontal;

  AddAttribute(tagConfig, &layer->sourceText, AttributeType::DiscreteProperty, defaultTextDocument);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
