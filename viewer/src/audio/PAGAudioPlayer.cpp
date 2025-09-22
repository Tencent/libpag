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
  connect(this, &PAGAudioPlayer::setVolumeInternal, audioRender.get(),
          &PAGAudioRender::setAudioVolume);
  connect(this, &PAGAudioPlayer::setIsPlayingInternal, audioReader.get(),
          &PAGAudioReader::onSetIsPlaying);
  connect(this, &PAGAudioPlayer::setIsPlayingInternal, audioRender.get(),
          &PAGAudioRender::onSetIsPlaying);
  connect(this, &PAGAudioPlayer::seekInternal, audioReader.get(), &PAGAudioReader::onSeek);
  connect(audioReader.get(), &PAGAudioReader::sendData, audioRender.get(), &PAGAudioRender::write);
}

void PAGAudioPlayer::setVolume(float volume) {
  Q_EMIT setVolumeInternal(volume);
}

void PAGAudioPlayer::setProgress(double percent) {
  if (pagFile == nullptr) {
    return;
  }
  auto time = static_cast<int64_t>(percent * pagFile->duration());
  Q_EMIT seekInternal(time);
}

void PAGAudioPlayer::setIsPlaying(bool isPlaying) {
  Q_EMIT setIsPlayingInternal(isPlaying);
}

void PAGAudioPlayer::setComposition(const std::shared_ptr<PAGFile>& pagFile) {
  this->pagFile = pagFile;
  audioReader->setComposition(std::dynamic_pointer_cast<PAGComposition>(this->pagFile));
  audioRender->flush();
}

}  // namespace pag