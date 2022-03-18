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
class CompositionMovie : public PAGMovie {
 public:
  ~CompositionMovie() override;

  int width() override;

  int height() override;

  int64_t duration() override;

  // internal methods start here:

  void draw(Recorder* recorder) override;

  void measureBounds(tgfx::Rect* bounds) override;

  bool setCurrentTime(int64_t time) override;

  bool excludedFromTimeline() const override;

  void invalidateCacheScale() override;

  void updateRootLocker(std::shared_ptr<std::mutex> newLocker) override;

  void onAddToStage(PAGStage* newStage) override;

  void onRemoveFromStage() override;

  int64_t durationInternal() override;

  std::vector<AudioClip> generateAudioClips() override;

 protected:
  tgfx::Rect getContentSize() const override;

 private:
  std::shared_ptr<PAGComposition> composition = nullptr;

  explicit CompositionMovie(std::shared_ptr<PAGComposition> composition);

  friend class PAGLayer;

  friend class PAGStage;

  friend class PAGMovie;
};
}  // namespace pag