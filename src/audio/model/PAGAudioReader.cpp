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

#ifdef FILE_MOVIE

#include "audio/model/AudioComposition.h"
#include "audio/reader/AudioReader.h"
#include "pag/file.h"
#include "pag/pag.h"

namespace pag {
std::shared_ptr<PAGAudioReader> PAGAudioReader::Make(std::shared_ptr<PAGAudio> audio,
                                                     int sampleRate, int sampleCount,
                                                     int channels) {
  if (audio == nullptr) {
    return nullptr;
  }
  auto outputSetting = std::make_shared<PCMOutputConfig>();
  outputSetting->sampleRate = sampleRate;
  outputSetting->outputSamplesCount = sampleCount;
  outputSetting->channels = channels;
  outputSetting->channelLayout = channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
  auto audioReader = AudioReader::Make(audio->audioComposition, outputSetting);
  if (audioReader == nullptr) {
    return nullptr;
  }
  std::shared_ptr<PAGAudioReader> reader(new PAGAudioReader(std::move(audio)));
  reader->audioReader = audioReader;
  return reader;
}

PAGAudioReader::PAGAudioReader(std::shared_ptr<PAGAudio> audio) : audio(std::move(audio)) {
  this->audio->addReader(this);
}

PAGAudioReader::~PAGAudioReader() {
  audio->removeReader(this);
}

void PAGAudioReader::checkAudioChanged() {
  if (!audioChanged) {
    return;
  }
  audioReader->onAudioChanged();
  audioChanged = false;
}

void PAGAudioReader::seek(int64_t toTime) {
  std::lock_guard<std::mutex> autoLock(audio->locker);
  checkAudioChanged();
  audioReader->seekTo(toTime);
}

std::shared_ptr<PAGAudioFrame> PAGAudioReader::copyNextFrame() {
  std::lock_guard<std::mutex> autoLock(audio->locker);
  checkAudioChanged();
  return audioReader->copyNextFrame();
}
}  // namespace pag
#endif
