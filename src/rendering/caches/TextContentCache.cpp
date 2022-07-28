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
#include "TextContent.h"
#include "base/utils/TGFXCast.h"
#include "rendering/graphics/Picture.h"
#include "rendering/graphics/Shape.h"
#include "rendering/graphics/Text.h"
#include "rendering/renderers/TextAnimatorRenderer.h"
#include "rendering/renderers/TextPathRender.h"
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

TextContentCache::TextContentCache(TextLayer* layer, ID cacheID,
                                   const std::vector<std::vector<GlyphHandle>>& lines)
    : ContentCache(layer), cacheID(cacheID), sourceText(layer->sourceText),
      pathOption(layer->pathOption), moreOption(layer->moreOption), animators(&layer->animators) {
  initTextGlyphs(&lines);
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
      scale *= std::max(1.f, std::max(value.x, value.y));
      for (const auto& keyframe : animatableProperty->keyframes) {
        value = keyframe->endValue;
        scale *= std::max(1.f, std::max(value.x, value.y));
      }
    } else {
      auto value = scaleProperty->value;
      scale *= std::max(1.f, std::max(value.x, value.y));
    }
  }
  return scale;
}

void TextContentCache::initTextGlyphs(const std::vector<std::vector<GlyphHandle>>* glyphLines) {
  auto scale = GetMaxScale(animators);
  if (glyphLines) {
    textBlock = std::make_shared<TextBlock>(getCacheID(), *glyphLines, scale);
    return;
  }
  auto addFunc = [&](TextDocument* textDocument, TextPathOptions* pathOptions) {
    auto [lines, bounds] = GetLines(textDocument, pathOptions);
    textBlocks[textDocument] = std::make_shared<TextBlock>(getCacheID(), lines, scale, &bounds);
  };
  if (sourceText->animatable()) {
    auto animatableProperty = reinterpret_cast<AnimatableProperty<TextDocumentHandle>*>(sourceText);
    addFunc(animatableProperty->keyframes[0]->startValue.get(), pathOption);
    for (const auto& keyframe : animatableProperty->keyframes) {
      addFunc(keyframe->endValue.get(), pathOption);
    }
  } else {
    addFunc(sourceText->getValueAt(0).get(), pathOption);
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

static std::vector<std::vector<GlyphHandle>> CopyLines(
    const std::shared_ptr<TextBlock>& textBlock) {
  std::vector<std::vector<GlyphHandle>> glyphLines;
  for (const auto& line : textBlock->lines()) {
    std::vector<GlyphHandle> glyphLine;
    glyphLine.reserve(line.size());
    for (const auto& glyph : line) {
      glyphLine.emplace_back(std::make_shared<Glyph>(*glyph));
    }
    glyphLines.emplace_back(glyphLine);
  }
  return glyphLines;
}

std::pair<std::vector<GlyphHandle>, std::vector<GlyphHandle>> GetGlyphs(
    const std::vector<std::vector<GlyphHandle>>& glyphLines) {
  std::vector<GlyphHandle> simpleGlyphs = {};
  std::vector<GlyphHandle> colorGlyphs = {};
  for (auto& line : glyphLines) {
    for (auto& glyph : line) {
      simpleGlyphs.push_back(glyph);
      auto typeface = glyph->getFont().getTypeface();
      if (typeface->hasColor()) {
        colorGlyphs.push_back(glyph);
      }
    }
  }
  return {simpleGlyphs, colorGlyphs};
}

GraphicContent* TextContentCache::createContent(Frame layerFrame) const {
  auto textDocument = sourceText->getValueAt(layerFrame).get();
  auto block = textBlock;
  if (block == nullptr) {
    auto iter = textBlocks.find(textDocument);
    if (iter == textBlocks.end()) {
      return nullptr;
    }
    block = iter->second;
  }
  auto glyphLines = CopyLines(block);
  bool toCalculateBounds = false;
  auto textPathRender = TextPathRender::MakeFrom(textDocument, pathOption);
  if (textPathRender != nullptr) {
    toCalculateBounds = true;
    // 强制对齐会导致重新排版
    textPathRender->applyForceAlignmentToGlyphs(glyphLines, layerFrame);
    // 由于动画的 position 是在文字路径上平移，而路径文字的映射是需要将排版文字的位置叠加动画位置结合路径进行计算，
    // 因此如果先应用路径，再计算动画，还需要计算一次动画位置，
    // 最好的方式是先应用动画，结合路径生成最后的矩阵
    TextAnimatorRenderer::ApplyToGlyphs(glyphLines, animators, textDocument->justification,
                                        layerFrame);
    textPathRender->applyToGlyphs(glyphLines, layerFrame);
  } else {
    toCalculateBounds = TextAnimatorRenderer::ApplyToGlyphs(
        glyphLines, animators, textDocument->justification, layerFrame);
  }

  std::vector<std::shared_ptr<Graphic>> contents = {};
  if (block != textBlock && textDocument->backgroundAlpha > 0) {
    auto background = RenderTextBackground(block->assetID(), glyphLines, textDocument);
    if (background) {
      contents.push_back(background);
    }
  }
  auto [simpleGlyphs, colorGlyphs] = GetGlyphs(glyphLines);
  const tgfx::Rect* textBounds = nullptr;
  if (!toCalculateBounds && !block->textBounds().isEmpty()) {
    textBounds = &block->textBounds();
  }

  auto normalText = Text::MakeFrom(simpleGlyphs, block, textBounds);
  if (normalText) {
    contents.push_back(normalText);
  }
  auto graphic = Graphic::MakeCompose(contents);
  auto colorText = Text::MakeFrom(colorGlyphs, block);
  auto content = new TextContent(std::move(graphic), std::move(colorText));
  if (_cacheEnabled) {
    content->colorGlyphs = Picture::MakeFrom(getCacheID(), content->colorGlyphs);
  }
  return content;
}
}  // namespace pag
