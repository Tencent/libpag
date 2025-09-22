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

#include "PAGAudioReader.h"
#include "model/AudioClip.h"

namespace pag {

static int64_t GetTimeNow() {
  static auto startTime = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime);
  return duration.count();
}

std::shared_ptr<PAGAudioReader> PAGAudioReader::Make(int sampleRate, int sampleCount, int channels,
                                                     float volume) {
  auto config = std::make_shared<AudioOutputConfig>();
  config->sampleRate = sampleRate;
  config->channels = channels;
  config->outputSamplesCount = sampleCount;
  return std::make_shared<PAGAudioReader>(config, volume);
}

PAGAudioReader::PAGAudioReader(const std::shared_ptr<AudioOutputConfig>& outputConfig, float volume)
    : volume(volume), outputConfig(outputConfig) {
  this->moveToThread(this);
  isRunning = true;
  connect(this, &PAGAudioReader::read, this, &PAGAudioReader::onRead);
  start();
}

PAGAudioReader::~PAGAudioReader() {
  isRunning = false;
  quit();
  wait();
}

bool PAGAudioReader::isEmpty() const {
  return empty;
}

void PAGAudioReader::setComposition(const std::shared_ptr<PAGComposition>& newComposition) {
  if (newComposition == composition) {
    return;
  }
  composition = newComposition;
  audioVersion = -1;
  updateAudioTrack();
  onSetIsPlaying(true);
}

void PAGAudioReader::setAudioRender(const std::shared_ptr<PAGAudioRender>& audioRender) {
  this->audioRender = audioRender;
}

std::shared_ptr<PAGAudioSample> PAGAudioReader::readNextSample() {
  updateAudioTrack();
  if (audioReader == nullptr) {
    return nullptr;
  }
  auto frame = audioReader->getNextSample();
  currentTime = frame->time;
  return frame;
}

void PAGAudioReader::onSeek(int64_t time) {
  currentTime = time;
  updateAudioTrack();
  if (audioReader != nullptr) {
    audioReader->seek(time);
  }
  if (isPlaying) {
    Q_EMIT read();
  }
}

void PAGAudioReader::onSetIsPlaying(bool isPlaying) {
  this->isPlaying = isPlaying;
  if (audioReader != nullptr) {
    audioReader->clearBuffer();
  }
  if (isPlaying) {
    Q_EMIT read();
  }
}

void PAGAudioReader::run() {
  while (isRunning) {
    QThread::exec();
  }
}

void PAGAudioReader::updateAudioTrack() {
  if (composition == nullptr) {
    return;
  }
  uint32_t currentVersion = composition->audioVersion;
  if (audioVersion == currentVersion) {
    return;
  }
  audioVersion = currentVersion;
  auto audio = std::make_shared<AudioAsset>();
  auto trackIDs = AudioClip::DumpTracks(composition, audio, volume);
  audioReader = AudioReader::Make(audio, outputConfig);
  audioReader->seek(currentTime);
  empty = trackIDs.empty();
}

void PAGAudioReader::onRead() {
  if (isPlaying) {
    int64_t delta = 0;
    auto sample = readNextSample();
    if (audioRender != nullptr) {
      int64_t current = GetTimeNow();
      delta = current - lastTime - audioRender->getAudioLatency();
    }
    if (delta < 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(-delta));
    }
    if (sample != nullptr) {
      sendData(std::move(sample));
    }
  }
}

}  // namespace pag
