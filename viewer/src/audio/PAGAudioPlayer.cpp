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

#include "PAGAudioPlayer.h"

namespace pag {

PAGAudioPlayer::PAGAudioPlayer() {
  AudioOutputConfig config;
  audioReader =
      PAGAudioReader::Make(config.sampleRate, config.outputSamplesCount, config.channels, 1.0);
  audioRender = PAGAudioRender::Make(config.sampleRate, config.channels);
  audioReader->setAudioRender(audioRender);
  connect(this, &PAGAudioPlayer::volumeChanged, audioRender.get(), &PAGAudioRender::setAudioVolume);
  connect(this, &PAGAudioPlayer::isPlayingChanged, audioReader.get(),
          &PAGAudioReader::onSetIsPlaying);
  connect(this, &PAGAudioPlayer::isPlayingChanged, audioRender.get(),
          &PAGAudioRender::onSetIsPlaying);
  connect(this, &PAGAudioPlayer::progressChanged, audioReader.get(),
          &PAGAudioReader::onPAGProgressChanged);
  connect(audioReader.get(), &PAGAudioReader::sendData, audioRender.get(), &PAGAudioRender::write);
  connect(audioReader.get(), &PAGAudioReader::audioTimeChanged, this,
          &PAGAudioPlayer::onAudioTimeChanged, Qt::QueuedConnection);
}

void PAGAudioPlayer::setVolume(float volume) {
  Q_EMIT volumeChanged(volume);
}

void PAGAudioPlayer::setProgress(double percent) {
  if (pagFile == nullptr) {
    return;
  }
  auto time = static_cast<int64_t>(percent * pagFile->duration());
  Q_EMIT progressChanged(time);
}

void PAGAudioPlayer::setIsPlaying(bool isPlaying) {
  Q_EMIT isPlayingChanged(isPlaying);
}

void PAGAudioPlayer::setComposition(std::shared_ptr<PAGFile> pagFile) {
  this->pagFile = std::move(pagFile);
  audioReader->setComposition(std::dynamic_pointer_cast<PAGComposition>(this->pagFile));
}

bool PAGAudioPlayer::isEmpty() const {
  return audioReader == nullptr || audioReader->isEmpty();
}

void PAGAudioPlayer::onAudioTimeChanged(int64_t audioTime) {
  if (pagFile != nullptr && audioTime > pagFile->duration()) {
    audioTime = 0;
    setProgress(0);
  }
  Q_EMIT audioTimeChanged(audioTime);
}

}  // namespace pag