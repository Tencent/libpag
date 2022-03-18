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

#include "CompositionMovie.h"
#include "audio/model/AudioClip.h"
#include "base/utils/TimeUtil.h"
#include "rendering/layers/PAGStage.h"
#include "rendering/utils/LockGuard.h"

namespace pag {
std::shared_ptr<PAGMovie> PAGMovie::FromComposition(std::shared_ptr<PAGComposition> composition) {
  if (composition == nullptr) {
    return nullptr;
  }
  LockGuard autoLock(composition->rootLocker);
  auto movie = new CompositionMovie(composition);
  return std::shared_ptr<PAGMovie>(movie);
}

CompositionMovie::CompositionMovie(std::shared_ptr<PAGComposition> composition)
    : composition(composition) {
  composition->removeFromParentOrOwner();
  composition->attachToTree(rootLocker, nullptr);
  composition->movieOwner = this;
}

CompositionMovie::~CompositionMovie() {
  if (composition) {
    composition->detachFromTree();
    composition->movieOwner = nullptr;
  }
}

int CompositionMovie::width() {
  return composition ? composition->width() : 0;
}

int CompositionMovie::height() {
  return composition ? composition->height() : 0;
}

int64_t CompositionMovie::duration() {
  if (composition == nullptr) {
    return 0;
  }
  return composition->startTime() + composition->duration();
}

int64_t CompositionMovie::durationInternal() {
  if (composition == nullptr) {
    return 0;
  }
  return composition->startTimeInternal() + composition->durationInternal();
}

tgfx::Rect CompositionMovie::getContentSize() const {
  if (composition == nullptr) {
    return tgfx::Rect::MakeEmpty();
  }
  return tgfx::Rect::MakeWH(composition->widthInternal(), composition->heightInternal());
}

void CompositionMovie::draw(Recorder* recorder) {
  if (composition) {
    composition->draw(recorder);
  }
}

void CompositionMovie::measureBounds(tgfx::Rect* bounds) {
  if (composition) {
    composition->measureBounds(bounds);
  }
}

bool CompositionMovie::setCurrentTime(int64_t time) {
  if (composition == nullptr) {
    return false;
  }
  return composition->gotoTime(time);
}

bool CompositionMovie::excludedFromTimeline() const {
  return composition ? composition->_excludedFromTimeline : true;
}

void CompositionMovie::invalidateCacheScale() {
  if (composition) {
    composition->invalidateCacheScale();
  }
}

void CompositionMovie::updateRootLocker(std::shared_ptr<std::mutex> newLocker) {
  PAGMovie::updateRootLocker(newLocker);
  if (composition) {
    composition->updateRootLocker(newLocker);
  }
}

void CompositionMovie::onAddToStage(PAGStage* newStage) {
  if (composition) {
    composition->onAddToStage(newStage);
  }
}

void CompositionMovie::onRemoveFromStage() {
  if (composition) {
    composition->onRemoveFromStage();
  }
}

std::vector<AudioClip> CompositionMovie::generateAudioClips() {
#ifdef FILE_MOVIE
  return AudioClip::GenerateAudioClips(composition.get());
#else
  return {};
#endif
}
}  // namespace pag
