/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <unordered_map>
#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
TextLayer::~TextLayer() {
  delete sourceText;
  delete pathOption;
  delete moreOption;
  for (auto& animator : animators) {
    delete animator;
  }
}

TextDocumentHandle TextLayer::getTextDocument() {
  if (sourceText == nullptr) {
    return nullptr;
  }
  return sourceText->getValueAt(startTime);
}

void TextLayer::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) {
  Layer::excludeVaryingRanges(timeRanges);
  sourceText->excludeVaryingRanges(timeRanges);
  if (pathOption) {
    pathOption->excludeVaryingRanges(timeRanges);
  }
  if (moreOption) {
    moreOption->excludeVaryingRanges(timeRanges);
  }
  for (auto& animator : animators) {
    animator->excludeVaryingRanges(timeRanges);
  }
}

bool TextLayer::verify() const {
  if (!Layer::verify()) {
    VerifyFailed();
    return false;
  }
  if (sourceText == nullptr) {
    VerifyFailed();
    return false;
  }
  if (pathOption != nullptr && !pathOption->verify()) {
    VerifyFailed();
    return false;
  }
  for (auto& animator : animators) {
    if (animator == nullptr || !animator->verify()) {
      VerifyFailed();
      return false;
    }
  }
  VerifyAndReturn(moreOption == nullptr || moreOption->verify());
}

Rect TextLayer::getBounds() const {
  return Rect::MakeWH(containingComposition->width, containingComposition->height);
}
}  // namespace pag
