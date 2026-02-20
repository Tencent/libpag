/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#ifdef PAG_USE_PAGX_LAYOUT

#include "BidiResolver.h"
#include <SheenBidi/SBAlgorithm.h>
#include <SheenBidi/SBBase.h>
#include <SheenBidi/SBCodepointSequence.h>
#include <SheenBidi/SBLine.h>
#include <SheenBidi/SBParagraph.h>
#include <SheenBidi/SBRun.h>

namespace pagx {

BidiResult BidiResolver::Resolve(const std::string& text, BaseDirection baseDirection) {
  if (text.empty()) {
    return {};
  }

  SBCodepointSequence codepointSequence = {};
  codepointSequence.stringEncoding = SBStringEncodingUTF8;
  codepointSequence.stringBuffer = text.data();
  codepointSequence.stringLength = static_cast<SBUInteger>(text.size());

  SBAlgorithmRef algorithm = SBAlgorithmCreate(&codepointSequence);
  if (algorithm == nullptr) {
    return {};
  }

  // Map BaseDirection to SheenBidi base level.
  // SBLevelDefaultLTR/RTL: detect from text per UAX#9 P2-P3, defaulting to the specified direction.
  // Explicit 0/1: force LTR/RTL regardless of text content.
  SBLevel baseLevel = SBLevelDefaultLTR;
  switch (baseDirection) {
    case BaseDirection::AutoLTR:
      baseLevel = SBLevelDefaultLTR;
      break;
    case BaseDirection::AutoRTL:
      baseLevel = SBLevelDefaultRTL;
      break;
    case BaseDirection::LTR:
      baseLevel = 0;
      break;
    case BaseDirection::RTL:
      baseLevel = 1;
      break;
  }

  // SheenBidi treats paragraph separators (\n, \r, etc.) as paragraph boundaries.
  // SBAlgorithmCreateParagraph only processes up to the first separator, so we must loop
  // over all paragraphs to cover the entire input text.
  BidiResult result = {};
  auto textLength = static_cast<SBUInteger>(text.size());
  SBUInteger paragraphOffset = 0;
  bool firstParagraph = true;

  while (paragraphOffset < textLength) {
    SBParagraphRef paragraph = SBAlgorithmCreateParagraph(
        algorithm, paragraphOffset, textLength - paragraphOffset, baseLevel);
    if (paragraph == nullptr) {
      break;
    }

    auto paragraphLength = SBParagraphGetLength(paragraph);
    if (firstParagraph) {
      result.isRTL = (SBParagraphGetBaseLevel(paragraph) % 2) != 0;
      firstParagraph = false;
    }
    SBLineRef line = SBParagraphCreateLine(paragraph, paragraphOffset, paragraphLength);
    if (line != nullptr) {
      auto runCount = SBLineGetRunCount(line);
      const SBRun* runs = SBLineGetRunsPtr(line);
      for (SBUInteger i = 0; i < runCount; ++i) {
        BidiRun run;
        run.start = static_cast<size_t>(runs[i].offset);
        run.length = static_cast<size_t>(runs[i].length);
        run.isRTL = (runs[i].level % 2) != 0;
        result.runs.push_back(run);
      }
      SBLineRelease(line);
    }

    paragraphOffset += paragraphLength;
    SBParagraphRelease(paragraph);
  }

  SBAlgorithmRelease(algorithm);

  return result;
}

}  // namespace pagx

#endif
