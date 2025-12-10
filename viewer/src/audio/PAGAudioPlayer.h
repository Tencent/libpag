/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include <QThread>
#include "PAGAudioReader.h"
#include "PAGAudioRender.h"
#include "pag/pag.h"

namespace pag {

class PAGAudioPlayer : public QObject {
  Q_OBJECT
 public:
  explicit PAGAudioPlayer();

  void setVolume(float volume);
  void setProgress(double percent);
  void setIsPlaying(bool isPlaying);
  void setComposition(std::shared_ptr<PAGFile> pagFile);
  bool isEmpty() const;

  Q_SIGNAL void audioTimeChanged(int64_t audioTime);

  Q_SLOT void onAudioTimeChanged(int64_t audioTime);

 private:
  Q_SIGNAL void progressChanged(int64_t time);
  Q_SIGNAL void isPlayingChanged(bool isPlaying);
  Q_SIGNAL void volumeChanged(float volume);

  std::shared_ptr<PAGFile> pagFile = nullptr;
  std::shared_ptr<PAGAudioReader> audioReader = nullptr;
  std::shared_ptr<PAGAudioRender> audioRender = nullptr;
};

}  // namespace pag