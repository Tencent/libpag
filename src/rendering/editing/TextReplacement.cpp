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
#include "rendering/caches/TextContentCache.h"
#include "rendering/utils/LockGuard.h"

namespace pag {

void TextReplacement::setAnimators(std::vector<pag::TextAnimator*>* animators) {
  LockGuard autoLock(pagLayer->rootLocker);
  clearCache();
  if (animators == nullptr) {
    *_animators = static_cast<TextLayer*>(pagLayer->layer)->animators;
  } else {
    *_animators = *animators;
  }
  pagLayer->notifyModified(true);
  pagLayer->invalidateCacheScale();
}

void TextReplacement::setLayoutGlyphs(const std::vector<GlyphHandle>& glyphs, Enum justification) {
  LockGuard autoLock(pagLayer->rootLocker);
  clearCache();
  std::string text;
  for (auto& g : glyphs) {
    text.append(g->getName());
  }
  auto textDocument = sourceText->value;
  if (!glyphs.empty()) {
    std::shared_ptr<MutableGlyph> firstGlyph = glyphs.front();
    textDocument->fontSize = firstGlyph->getFont().getSize();
    textDocument->fauxBold = firstGlyph->getFont().isFauxBold();
    textDocument->fauxItalic = firstGlyph->getFont().isFauxItalic();
    textDocument->strokeColor = firstGlyph->getStrokeColor();
    textDocument->strokeWidth = firstGlyph->getStrokeWidth();
    textDocument->strokeOverFill = firstGlyph->getStrokeOverFill();
    textDocument->fillColor = firstGlyph->getFillColor();
    textDocument->direction =
        firstGlyph->isVertical() ? TextDirection::Vertical : TextDirection::Default;
  }
  textDocument->text = text;
  textDocument->justification = justification;
  layoutGlyphs = glyphs;
  if (textContentCache) {
    textContentCache->updateStaticTimeRanges();
  }
  pagLayer->notifyModified(true);
  pagLayer->invalidateCacheScale();
}

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
}

Content* TextReplacement::getContent(Frame contentFrame) {
  if (textContentCache == nullptr) {
    auto textLayer = static_cast<TextLayer*>(pagLayer->layer);
    textContentCache =
        new TextContentCache(textLayer, pagLayer->uniqueID(), sourceText, _animators, layoutGlyphs);
    textContentCache->update();
  }
  return textContentCache->getCache(contentFrame);
}

TextDocument* TextReplacement::getTextDocument() {
  return sourceText->value.get();
}

void TextReplacement::resetText() {
  sourceText->value = std::make_shared<TextDocument>();
  *sourceText->value = *static_cast<TextLayer*>(pagLayer->layer)->sourceText->value;
}

void TextReplacement::clearCache() {
  delete textContentCache;
  textContentCache = nullptr;
}

TextContentCache* TextReplacement::contentCache() {
  return textContentCache;
}

}  // namespace pag
