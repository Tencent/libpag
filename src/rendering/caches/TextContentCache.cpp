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
    : ContentCache(layer),
      sourceText(layer->sourceText),
      pathOption(layer->pathOption),
      moreOption(layer->moreOption),
      animators(&layer->animators) {
}

TextContentCache::TextContentCache(TextLayer* layer, ID cacheID,
                                   Property<TextDocumentHandle>* sourceText)
    : ContentCache(layer),
      cacheID(cacheID),
      sourceText(sourceText),
      pathOption(layer->pathOption),
      moreOption(layer->moreOption),
      animators(&layer->animators) {
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
  auto content = RenderTexts(sourceText, pathOption, moreOption, animators, layerFrame).release();
  if (_cacheEnabled) {
    content->colorGlyphs = Picture::MakeFrom(getCacheID(), content->colorGlyphs);
  }
  return content;
}

}  // namespace pag