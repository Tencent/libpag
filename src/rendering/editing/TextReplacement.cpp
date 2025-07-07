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

#include "TextReplacement.h"

namespace pag {
TextReplacement::TextReplacement(PAGTextLayer* pagLayer) : pagLayer(pagLayer) {
  auto textLayer = static_cast<TextLayer*>(pagLayer->layer);
  sourceText = new Property<TextDocumentHandle>();
  auto textData = TextDocumentHandle(new TextDocument());
  *textData = *(textLayer->sourceText->value);
  sourceText->value = textData;
}

TextReplacement::~TextReplacement() {
  delete textContentCache;
  delete sourceText;
}

Content* TextReplacement::getContent(Frame contentFrame) {
  if (textContentCache == nullptr) {
    auto textLayer = static_cast<TextLayer*>(pagLayer->layer);
    textContentCache = new TextContentCache(textLayer, pagLayer->uniqueID(), sourceText);
    textContentCache->update();
  }
  return textContentCache->getCache(contentFrame);
}

TextDocument* TextReplacement::getTextDocument() {
  return sourceText->value.get();
}

void TextReplacement::clearCache() {
  delete textContentCache;
  textContentCache = nullptr;
}
}  // namespace pag
