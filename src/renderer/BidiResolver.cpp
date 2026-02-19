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

std::vector<BidiRun> BidiResolver::Resolve(const std::string& text) {
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

  SBParagraphRef paragraph =
      SBAlgorithmCreateParagraph(algorithm, 0, static_cast<SBUInteger>(text.size()),
                                 SBLevelDefaultLTR);
  if (paragraph == nullptr) {
    SBAlgorithmRelease(algorithm);
    return {};
  }

  auto paragraphLength = SBParagraphGetLength(paragraph);
  SBLineRef line = SBParagraphCreateLine(paragraph, 0, paragraphLength);
  if (line == nullptr) {
    SBParagraphRelease(paragraph);
    SBAlgorithmRelease(algorithm);
    return {};
  }

  auto runCount = SBLineGetRunCount(line);
  const SBRun* runs = SBLineGetRunsPtr(line);

  std::vector<BidiRun> result;
  result.reserve(static_cast<size_t>(runCount));
  for (SBUInteger i = 0; i < runCount; ++i) {
    BidiRun run;
    run.start = static_cast<size_t>(runs[i].offset);
    run.length = static_cast<size_t>(runs[i].length);
    run.isRTL = (runs[i].level % 2) != 0;
    result.push_back(run);
  }

  SBLineRelease(line);
  SBParagraphRelease(paragraph);
  SBAlgorithmRelease(algorithm);

  return result;
}

}  // namespace pagx

#endif
