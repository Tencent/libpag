/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
  sourceText->value = std::make_shared<TextDocument>();
  *sourceText->value = *textLayer->sourceText->value;
  _animators = new std::vector<TextAnimator*>;
  *_animators = textLayer->animators;
}

TextReplacement::~TextReplacement() {
  delete textContentCache;
  delete sourceText;
  delete _animators;
  delete layoutGlyphs;
}

Content* TextReplacement::getContent(Frame contentFrame) {
  if (textContentCache == nullptr) {
    auto textLayer = static_cast<TextLayer*>(pagLayer->layer);
    if (layoutGlyphs) {
      textContentCache = new TextContentCache(textLayer, pagLayer->uniqueID(), sourceText,
                                              _animators, *layoutGlyphs);
    } else {
      textContentCache =
          new TextContentCache(textLayer, pagLayer->uniqueID(), sourceText, _animators);
    }
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

void TextReplacement::setLayoutGlyphs(
    const std::vector<std::vector<std::shared_ptr<Glyph>>>& glyphs) {
  clearCache();
  if (!layoutGlyphs) {
    layoutGlyphs = new std::vector<std::vector<std::shared_ptr<Glyph>>>();
  }
  *layoutGlyphs = glyphs;
  pagLayer->notifyModified(true);
  pagLayer->invalidateCacheScale();
}

std::vector<TextAnimator*>* TextReplacement::animators() {
  return _animators;
}

void TextReplacement::setAnimators(std::vector<TextAnimator*>* animators) {
  clearCache();
  if (animators == nullptr) {
    *_animators = static_cast<TextLayer*>(pagLayer->layer)->animators;
  } else {
    *_animators = *animators;
  }
  pagLayer->notifyModified(true);
  pagLayer->invalidateCacheScale();
}

TextContentCache* TextReplacement::contentCache() {
  return textContentCache;
}

}  // namespace pag
