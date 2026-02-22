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

#ifdef PAG_BUILD_PAGX

#include "BidiResolver.h"
#include <SheenBidi/SBAlgorithm.h>
#include <SheenBidi/SBBase.h>
#include <SheenBidi/SBCodepointSequence.h>
#include <SheenBidi/SBParagraph.h>

namespace pagx {

// Decode one UTF-8 character and return the number of bytes consumed.
static size_t UTF8CharLength(const char* data, size_t remaining) {
  if (remaining == 0) {
    return 0;
  }
  auto byte = static_cast<uint8_t>(data[0]);
  if (byte < 0x80) {
    return 1;
  }
  if ((byte & 0xE0) == 0xC0 && remaining >= 2) {
    return 2;
  }
  if ((byte & 0xF0) == 0xE0 && remaining >= 3) {
    return 3;
  }
  if ((byte & 0xF8) == 0xF0 && remaining >= 4) {
    return 4;
  }
  return 1;
}

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
    SBParagraphRef paragraph = SBAlgorithmCreateParagraph(algorithm, paragraphOffset,
                                                          textLength - paragraphOffset, baseLevel);
    if (paragraph == nullptr) {
      break;
    }

    auto paragraphLength = SBParagraphGetLength(paragraph);
    if (firstParagraph) {
      result.isRTL = (SBParagraphGetBaseLevel(paragraph) % 2) != 0;
      firstParagraph = false;
    }

    // Build logical-order runs from per-character bidi levels. SBParagraphGetLevelsPtr returns
    // one level per byte for UTF-8; characters that span multiple bytes have the same level on
    // each byte. We advance by full UTF-8 character widths and start a new run when the level
    // changes.
    const SBLevel* levels = SBParagraphGetLevelsPtr(paragraph);
    size_t pos = 0;
    while (pos < paragraphLength) {
      SBLevel currentLevel = levels[pos];
      size_t runStart = pos;
      while (pos < paragraphLength && levels[pos] == currentLevel) {
        size_t charLen = UTF8CharLength(text.data() + paragraphOffset + pos, paragraphLength - pos);
        if (charLen == 0) {
          break;
        }
        pos += charLen;
      }
      BidiRun run = {};
      run.start = static_cast<size_t>(paragraphOffset) + runStart;
      run.length = pos - runStart;
      run.level = static_cast<uint8_t>(currentLevel);
      result.runs.push_back(run);
    }

    paragraphOffset += paragraphLength;
    SBParagraphRelease(paragraph);
  }

  SBAlgorithmRelease(algorithm);

  return result;
}

}  // namespace pagx

#endif
