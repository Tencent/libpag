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
VectorComposition::~VectorComposition() {
  for (auto& layer : layers) {
    delete layer;
  }
}

CompositionType VectorComposition::type() const {
  return CompositionType::Vector;
}

void VectorComposition::updateStaticTimeRanges() {
  staticTimeRanges = {};
  if (duration <= 1) {
    return;
  }
  TimeRange range = {0, duration - 1};
  staticTimeRanges.push_back(range);
  for (auto layer : layers) {
    if (staticTimeRanges.empty()) {
      break;
    }
    if (layer->type() == LayerType::PreCompose) {
      auto composition = static_cast<PreComposeLayer*>(layer)->composition;
      if (!composition->staticTimeRangeUpdated) {
        composition->updateStaticTimeRanges();
        composition->staticTimeRangeUpdated = true;
      }
    }
    layer->excludeVaryingRanges(&staticTimeRanges);
    SplitTimeRangesAt(&staticTimeRanges, layer->startTime);
    SplitTimeRangesAt(&staticTimeRanges, layer->startTime + layer->duration);
  }
}

bool VectorComposition::hasImageContent() const {
  for (auto& layer : layers) {
    switch (layer->type()) {
      case LayerType::PreCompose:
        if (static_cast<PreComposeLayer*>(layer)->composition->hasImageContent()) {
          return true;
        }
        break;
      case LayerType::Image:
        return true;
      default:
        break;
    }
  }
  return false;
}

bool VectorComposition::verify() const {
  if (!Composition::verify()) {
    VerifyFailed();
    return false;
  }
  for (auto layer : layers) {
    if (layer == nullptr || !layer->verify()) {
      VerifyFailed();
      return false;
    }
  }
  return true;
}
}  // namespace pag
