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

#include "pag/file.h"
#include "pag/pag.h"

namespace pag {

class TextContentCache;
class MutableGlyph;

class TextReplacement {
 public:
  /**
   * Set animators to PAGTextLayer.
   * The pointer of PAGFile which animators came from must be referenced by caller.
   */
  void setAnimators(std::vector<TextAnimator*>* animators);

  /**
   * Set typography information to PAGTextLayer.
   * Enum of justification is ParagraphJustification.
   */
  void setLayoutGlyphs(const std::vector<std::shared_ptr<MutableGlyph>>& glyphs,
                       Enum justification);

 private:
  explicit TextReplacement(PAGTextLayer* textLayer);
  ~TextReplacement();
  void clearCache();
  void resetText();
  TextContentCache* contentCache();
  Content* getContent(Frame contentFrame);

  TextDocument* getTextDocument();
  TextContentCache* textContentCache = nullptr;
  Property<TextDocumentHandle>* sourceText = nullptr;
  PAGTextLayer* pagLayer = nullptr;
  std::vector<TextAnimator*>* _animators = nullptr;
  std::vector<std::shared_ptr<MutableGlyph>> layoutGlyphs;
  friend class PAGTextLayer;
};
}  // namespace pag
