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

#include "base/utils/TimeUtil.h"
#include "pag/pag.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/TextContentCache.h"
#include "rendering/editing/TextReplacement.h"
#include "rendering/utils/LockGuard.h"

namespace pag {
std::shared_ptr<PAGTextLayer> PAGTextLayer::Make(int64_t duration, std::string text, float fontSize,
                                                 std::string fontFamily, std::string fontStyle) {
  if (duration <= 0) {
    return nullptr;
  }
  auto textDocument = std::shared_ptr<TextDocument>(new TextDocument());
  textDocument->text = std::move(text);
  textDocument->fontSize = fontSize;
  textDocument->fontFamily = std::move(fontFamily);
  textDocument->fontStyle = std::move(fontStyle);
  return Make(duration, textDocument);
}

std::shared_ptr<PAGTextLayer> PAGTextLayer::Make(int64_t duration,
                                                 std::shared_ptr<TextDocument> textDocumentHandle) {
  if (duration <= 0 || textDocumentHandle == nullptr) {
    return nullptr;
  }
  auto layer = new TextLayer();
  auto transform = Transform2D::MakeDefault();
  transform->position->value = Point::Make(0, textDocumentHandle->fontSize);
  layer->transform = transform;
  auto sourceText = new Property<TextDocumentHandle>();
  sourceText->value = std::move(textDocumentHandle);
  layer->sourceText = sourceText;

  layer->duration = TimeToFrame(duration, 60);
  auto textLayer = std::make_shared<PAGTextLayer>(nullptr, layer);
  textLayer->emptyTextLayer = layer;
  textLayer->weakThis = textLayer;
  return textLayer;
}

PAGTextLayer::PAGTextLayer(std::shared_ptr<pag::File> file, TextLayer* layer)
    : PAGLayer(file, layer) {
}

PAGTextLayer::~PAGTextLayer() {
  delete replacement;
  delete emptyTextLayer;
}

Color PAGTextLayer::fillColor() const {
  LockGuard autoLock(rootLocker);
  return textDocumentForRead()->fillColor;
}

void PAGTextLayer::setFillColor(const Color& value) {
  LockGuard autoLock(rootLocker);
  textDocumentForWrite()->fillColor = value;
}

PAGFont PAGTextLayer::font() const {
  LockGuard autoLock(rootLocker);
  auto textDocument = textDocumentForRead();
  return {textDocument->fontFamily, textDocument->fontStyle};
}

void PAGTextLayer::setFont(const PAGFont& font) {
  LockGuard autoLock(rootLocker);
  auto textDocument = textDocumentForWrite();
  textDocument->fontFamily = font.fontFamily;
  textDocument->fontStyle = font.fontStyle;
}

float PAGTextLayer::fontSize() const {
  LockGuard autoLock(rootLocker);
  return textDocumentForRead()->fontSize;
}

void PAGTextLayer::setFontSize(float size) {
  LockGuard autoLock(rootLocker);
  textDocumentForWrite()->fontSize = size;
}

Color PAGTextLayer::strokeColor() const {
  LockGuard autoLock(rootLocker);
  return textDocumentForRead()->strokeColor;
}

void PAGTextLayer::setStrokeColor(const Color& color) {
  LockGuard autoLock(rootLocker);
  textDocumentForWrite()->strokeColor = color;
}

std::string PAGTextLayer::text() const {
  LockGuard autoLock(rootLocker);
  return textDocumentForRead()->text;
}

void PAGTextLayer::setText(const std::string& text) {
  LockGuard autoLock(rootLocker);
  textDocumentForWrite()->text = text;
}

void PAGTextLayer::replaceTextInternal(std::shared_ptr<TextDocument> textData) {
  if (textData == nullptr) {
    if (replacement != nullptr) {
      replacement->resetText();
      notifyModified(true);
      invalidateCacheScale();
    }
  } else {
    auto textDocument = textDocumentForWrite();
    // 仅以下属性支持外部修改：
    textDocument->applyFill = textData->applyFill;
    textDocument->applyStroke = textData->applyStroke;
    textDocument->fauxBold = textData->fauxBold;
    textDocument->fauxItalic = textData->fauxItalic;
    textDocument->fillColor = textData->fillColor;
    textDocument->fontFamily = textData->fontFamily;
    textDocument->fontStyle = textData->fontStyle;
    textDocument->fontSize = textData->fontSize;
    textDocument->strokeColor = textData->strokeColor;
    textDocument->strokeWidth = textData->strokeWidth;
    textDocument->text = textData->text;
    textDocument->backgroundColor = textData->backgroundColor;
    textDocument->backgroundAlpha = textData->backgroundAlpha;
    textDocument->justification = textData->justification;
  }
}

const TextDocument* PAGTextLayer::textDocumentForRead() const {
  return replacement ? replacement->getTextDocument()
                     : static_cast<TextLayer*>(layer)->sourceText->value.get();
}

TextDocument* PAGTextLayer::textDocumentForWrite() {
  if (replacement == nullptr) {
    replacement = new TextReplacement(this);
  } else {
    replacement->clearCache();
  }
  notifyModified(true);
  invalidateCacheScale();
  return replacement->getTextDocument();
}

void PAGTextLayer::reset() {
  if (replacement != nullptr) {
    delete replacement;
    replacement = nullptr;
    notifyModified(true);
    invalidateCacheScale();
  }
}

Content* PAGTextLayer::getContent() {
  if (replacement != nullptr) {
    return replacement->getContent(contentFrame);
  }
  return layerCache->getContent(contentFrame);
}

bool PAGTextLayer::contentModified() const {
  return replacement != nullptr;
}

void PAGTextLayer::setMatrixInternal(const Matrix& matrix) {
  if (matrix == layerMatrix) {
    return;
  }
  PAGLayer::setMatrixInternal(matrix);
}

bool PAGTextLayer::gotoTime(int64_t layerTime) {
  auto changed = false;
  if (_trackMatteLayer != nullptr) {
    changed = _trackMatteLayer->gotoTime(layerTime);
  }
  auto layerFrame = TimeToFrame(layerTime, frameRateInternal());
  auto oldContentFrame = contentFrame;
  contentFrame = layerFrame - startFrame;
  if (!changed) {
    if (replacement && replacement->contentCache()) {
      changed = replacement->contentCache()->checkFrameChanged(contentFrame, oldContentFrame);
    } else {
      changed = layerCache->checkFrameChanged(contentFrame, oldContentFrame);
    }
  }
  return changed;
}

TextReplacement* PAGTextLayer::textReplacementForWrite() {
  pag::LockGuard autoLock(rootLocker);
  if (replacement == nullptr) {
    replacement = new TextReplacement(this);
  }
  return replacement;
}

std::vector<TextAnimator*> PAGTextLayer::textAnimators() {
  return static_cast<TextLayer*>(layer)->animators;
}

}  // namespace pag
