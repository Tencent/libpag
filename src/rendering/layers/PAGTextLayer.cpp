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
#include "rendering/editing/ReplacementHolder.h"
#include "rendering/editing/TextReplacement.h"
#include "rendering/layers/PAGStage.h"
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
  textLayer->_editableIndex = 0;
  textLayer->textHolder = std::make_shared<ReplacementHolder<TextDocument>>();
  textLayer->textHolder->addLayer(textLayer.get());
  textLayer->replacement =
      new TextReplacement(textLayer.get(), textLayer->textHolder.get(), textLayer->_editableIndex);
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
  auto textDocument = textDocumentForWrite();
  if (textDocument == nullptr) {
    return;
  }
  textDocument->fillColor = value;
}

PAGFont PAGTextLayer::font() const {
  LockGuard autoLock(rootLocker);
  auto textDocument = textDocumentForRead();
  return {textDocument->fontFamily, textDocument->fontStyle};
}

void PAGTextLayer::setFont(const PAGFont& font) {
  LockGuard autoLock(rootLocker);
  auto textDocument = textDocumentForWrite();
  if (textDocument == nullptr) {
    return;
  }
  textDocument->fontFamily = font.fontFamily;
  textDocument->fontStyle = font.fontStyle;
}

float PAGTextLayer::fontSize() const {
  LockGuard autoLock(rootLocker);
  return textDocumentForRead()->fontSize;
}

void PAGTextLayer::setFontSize(float size) {
  LockGuard autoLock(rootLocker);
  auto textDocument = textDocumentForWrite();
  if (textDocument == nullptr) {
    return;
  }
  textDocument->fontSize = size;
}

Color PAGTextLayer::strokeColor() const {
  LockGuard autoLock(rootLocker);
  return textDocumentForRead()->strokeColor;
}

void PAGTextLayer::setStrokeColor(const Color& color) {
  LockGuard autoLock(rootLocker);
  auto textDocument = textDocumentForWrite();
  if (textDocument == nullptr) {
    return;
  }
  textDocument->strokeColor = color;
}

std::string PAGTextLayer::text() const {
  LockGuard autoLock(rootLocker);
  return textDocumentForRead()->text;
}

void PAGTextLayer::setText(const std::string& text) {
  LockGuard autoLock(rootLocker);
  auto textDocument = textDocumentForWrite();
  if (textDocument == nullptr) {
    return;
  }
  textDocument->text = text;
}

void PAGTextLayer::replaceTextInternal(std::shared_ptr<TextDocument> textData) {
  if (textData == nullptr) {
    reset();
  } else {
    auto textDocument = textDocumentForWrite();
    if (textDocument == nullptr) {
      return;
    }
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
  return contentModified() ? replacement->getTextDocument()
                           : static_cast<TextLayer*>(layer)->sourceText->value.get();
}

TextDocument* PAGTextLayer::textDocumentForWrite() {
  if (textHolder == nullptr) {
    return nullptr;
  }
  replacement->clearCache();
  notifyReferenceLayers();
  return replacement->getTextDocument();
}

void PAGTextLayer::reset() {
  if (contentModified()) {
    textHolder->setReplacement(_editableIndex, nullptr, this);
    replacement->clearCache();
    notifyReferenceLayers();
  }
}

Content* PAGTextLayer::getContent() {
  if (contentModified()) {
    return replacement->getContent(contentFrame);
  }
  return layerCache->getContent(contentFrame);
}

bool PAGTextLayer::contentModified() const {
  return textHolder && textHolder->hasReplacement(_editableIndex);
}

void PAGTextLayer::onAddToRootFile(PAGFile* pagFile) {
  PAGLayer::onAddToRootFile(pagFile);
  textHolder = pagFile->textHolder;
  textHolder->addLayer(this);
  replacement = new TextReplacement(this, textHolder.get(), _editableIndex);
}

void PAGTextLayer::onRemoveFromRootFile() {
  PAGLayer::onRemoveFromRootFile();
  textHolder->removeLayer(this);
  delete replacement;
  replacement = nullptr;
  textHolder = nullptr;
}

void PAGTextLayer::setMatrixInternal(const Matrix& matrix) {
  if (matrix == layerMatrix) {
    return;
  }
  PAGLayer::setMatrixInternal(matrix);
}

void PAGTextLayer::notifyReferenceLayers() {
  std::vector<PAGTextLayer*> textLayers = {};
  if (rootFile) {
    auto layers = rootFile->getLayersByEditableIndexInternal(_editableIndex, LayerType::Text);
    for (auto& pagLayer : layers) {
      textLayers.push_back(static_cast<PAGTextLayer*>(pagLayer.get()));
    }
  } else {
    textLayers.push_back(this);
  }
  std::for_each(textLayers.begin(), textLayers.end(), [](PAGTextLayer* textLayer) {
    textLayer->notifyModified(true);
    textLayer->invalidateCacheScale();
  });
}

}  // namespace pag
