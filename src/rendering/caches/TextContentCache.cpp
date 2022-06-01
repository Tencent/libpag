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

#include "TextContentCache.h"
#include "rendering/graphics/Picture.h"
#include "rendering/renderers/TextRenderer.h"

namespace pag {
TextContentCache::TextContentCache(TextLayer* layer)
    : ContentCache(layer), sourceText(layer->sourceText), pathOption(layer->pathOption),
      moreOption(layer->moreOption), animators(&layer->animators) {
  initTextGlyphs();
}

TextContentCache::TextContentCache(TextLayer* layer, ID cacheID,
                                   Property<TextDocumentHandle>* sourceText)
    : ContentCache(layer), cacheID(cacheID), sourceText(sourceText), pathOption(layer->pathOption),
      moreOption(layer->moreOption), animators(&layer->animators) {
  initTextGlyphs();
}

static float GetMaxScale(std::vector<TextAnimator*>* animators) {
  if (animators == nullptr) {
    return 1.0f;
  }
  float scale = 1.0f;
  for (auto* animator : *animators) {
    Property<Point>* scaleProperty = nullptr;
    if (animator->typographyProperties) {
      scaleProperty = animator->typographyProperties->scale;
    }
    if (scaleProperty == nullptr) {
      continue;
    }
    if (scaleProperty->animatable()) {
      auto animatableProperty = static_cast<AnimatableProperty<Point>*>(scaleProperty);
      auto value = animatableProperty->keyframes[0]->startValue;
      scale *= std::max(value.x, value.y);
      for (const auto& keyframe : animatableProperty->keyframes) {
        value = keyframe->endValue;
        scale *= std::max(value.x, value.y);
      }
    } else {
      auto value = scaleProperty->value;
      scale *= std::max(value.x, value.y);
    }
  }
  return scale;
}

void TextContentCache::initTextGlyphs() {
  auto scale = GetMaxScale(animators);
  auto addFunc = [&](TextDocument* textDocument) {
    auto [lines, bounds] = GetLines(textDocument);
    textGlyphs[textDocument] = std::make_shared<TextGlyphs>(getCacheID(), lines, bounds, scale);
  };
  if (sourceText->animatable()) {
    auto animatableProperty = reinterpret_cast<AnimatableProperty<TextDocumentHandle>*>(sourceText);
    addFunc(animatableProperty->keyframes[0]->startValue.get());
    for (const auto& keyframe : animatableProperty->keyframes) {
      addFunc(keyframe->endValue.get());
    }
  } else {
    addFunc(sourceText->getValueAt(0).get());
  }
}

void TextContentCache::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  sourceText->excludeVaryingRanges(timeRanges);
  if (pathOption) {
    pathOption->excludeVaryingRanges(timeRanges);
  }
  if (moreOption) {
    moreOption->excludeVaryingRanges(timeRanges);
  }
  for (auto animator : *animators) {
    animator->excludeVaryingRanges(timeRanges);
  }
}

ID TextContentCache::getCacheID() const {
  return cacheID > 0 ? cacheID : layer->uniqueID;
}

GraphicContent* TextContentCache::createContent(Frame layerFrame) const {
  auto textDocument = sourceText->getValueAt(layerFrame);
  auto iter = textGlyphs.find(textDocument.get());
  if (iter == textGlyphs.end()) {
    return nullptr;
  }
  auto content = RenderTexts(iter->second, pathOption, moreOption, animators, layerFrame,
                             textDocument->backgroundColor, textDocument->backgroundAlpha)
                     .release();
  if (_cacheEnabled) {
    content->colorGlyphs = Picture::MakeFrom(getCacheID(), content->colorGlyphs);
  }
  return content;
}
}  // namespace pag
