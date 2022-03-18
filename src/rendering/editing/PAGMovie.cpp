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

#include "pag/file.h"
#include "pag/pag.h"

namespace pag {
bool PAGMovie::isMovie() const {
  return true;
}

bool PAGMovie::excludedFromTimeline() const {
  return false;
}

void PAGMovie::invalidateCacheScale() {
}

void PAGMovie::updateRootLocker(std::shared_ptr<std::mutex> newLocker) {
  rootLocker = newLocker;
}

void PAGMovie::onAddToStage(PAGStage*) {
}

void PAGMovie::onRemoveFromStage() {
}

void PAGMovie::setVolumeRamp(float, float, const TimeRange&) {
}
}  // namespace pag
