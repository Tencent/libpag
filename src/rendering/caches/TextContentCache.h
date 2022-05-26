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
#include "TextGlyphs.h"

namespace pag {
class TextContentCache : public ContentCache {
 public:
  explicit TextContentCache(TextLayer* layer);
  TextContentCache(TextLayer* layer, ID cacheID, Property<TextDocumentHandle>* sourceText,
                   std::vector<TextAnimator*>* animators,
                   const std::vector<GlyphHandle>& layoutGlyphs = {});
  void updateStaticTimeRanges();
  bool checkFrameChanged(Frame contentFrame, Frame lastContentFrame);
  void setAnimators(std::vector<pag::TextAnimator*>* animators);

 protected:
  void excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const override;
  ID getCacheID() const override;
  GraphicContent* createContent(Frame layerFrame) const override;

 private:
  void initTextGlyphs();

  ID cacheID = 0;
  Property<TextDocumentHandle>* sourceText;
  TextPathOptions* pathOption;
  TextMoreOptions* moreOption;
  std::vector<TextAnimator*>* _animators = nullptr;
  std::vector<GlyphHandle> layoutGlyphs = {};
  std::unordered_map<TextDocument*, std::shared_ptr<TextGlyphs>> textGlyphs;
  std::vector<TimeRange> staticTimeRanges;
};
}  // namespace pag
