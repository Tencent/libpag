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

#pragma once

#include <unordered_map>
#include "ContentCache.h"
#include "TextBlock.h"

namespace pag {
class TextContentCache : public ContentCache {
 public:
  explicit TextContentCache(TextLayer* layer);
  TextContentCache(TextLayer* layer, ID cacheID, Property<TextDocumentHandle>* sourceText);
  TextContentCache(TextLayer* layer, ID cacheID,
                   const std::vector<std::vector<GlyphHandle>>& lines);

 protected:
  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;
  ID getCacheID() const override;
  GraphicContent* createContent(Frame layerFrame) const override;

 private:
  void initTextGlyphs(const std::vector<std::vector<GlyphHandle>>* glyphLines = nullptr);

  ID cacheID = 0;
  Property<TextDocumentHandle>* sourceText;
  TextPathOptions* pathOption;
  TextMoreOptions* moreOption;
  std::vector<TextAnimator*>* animators;
  std::unordered_map<TextDocument*, std::shared_ptr<TextBlock>> textBlocks;
  std::shared_ptr<TextBlock> textBlock;
};
}  // namespace pag
