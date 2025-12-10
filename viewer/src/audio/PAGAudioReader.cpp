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
#include <tgfx/core/Clock.h>
#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <utility>
#include "model/AudioClip.h"
#include "utils/AudioUtils.h"

namespace pag {

constexpr int64_t TimeLagNeedToUpdate = 50 * 1000;
constexpr int64_t TimeAheadTolerance = 5 * 1000;

std::shared_ptr<PAGAudioReader> PAGAudioReader::Make(int sampleRate, int sampleCount, int channels,
                                                     float volume) {
  auto config = std::make_shared<AudioOutputConfig>();
  config->sampleRate = sampleRate;
  config->channels = channels;
  config->outputSamplesCount = sampleCount;
  return std::make_shared<PAGAudioReader>(config, volume);
}

PAGAudioReader::PAGAudioReader(std::shared_ptr<AudioOutputConfig> outputConfig, float volume)
    : volume(volume), outputConfig(std::move(outputConfig)) {
  this->moveToThread(this);
  isRunning = true;
  connect(this, &PAGAudioReader::read, this, &PAGAudioReader::onRead, Qt::QueuedConnection);
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

void PAGAudioReader::setComposition(std::shared_ptr<PAGComposition> newComposition) {
  if (newComposition == composition) {
    return;
  }
  composition = std::move(newComposition);
  lastTime = tgfx::Clock::Now();
  lastSampleTime = 0;
  currentProgress = 0;
  updateAudioTrack();
  onSetIsPlaying(true);
}

void PAGAudioReader::setAudioRender(std::shared_ptr<PAGAudioRender> audioRender) {
  this->audioRender = std::move(audioRender);
}

std::shared_ptr<PAGAudioSample> PAGAudioReader::readNextSample() {
  if (audioReader == nullptr) {
    return nullptr;
  }
  auto sample = audioReader->getNextSample();
  currentProgress = sample->time;
  return sample;
}

void PAGAudioReader::onPAGProgressChanged(int64_t progress) {
  if (empty || composition == nullptr) {
    return;
  }
  audioReader->seek(progress);
  audioReader->clearBuffer();

  auto sample = audioReader->getNextSample();
  if (sample != nullptr) {
    currentProgress = sample->time;
    lastSampleTime = sample->time;
  } else {
    currentProgress = progress;
    lastSampleTime = progress;
  }
}

void PAGAudioReader::onSetIsPlaying(bool isPlaying) {
  this->isPlaying = isPlaying;
  if (audioReader != nullptr) {
    audioReader->clearBuffer();
  }
  if (isPlaying) {
    updateClock(false);
    Q_EMIT read();
  } else {
    updateClock(true);
  }
}

void PAGAudioReader::run() {
  QThread::exec();
}

void PAGAudioReader::updateAudioTrack() {
  if (composition == nullptr) {
    return;
  }
  auto audio = std::make_shared<AudioAsset>();
  AudioClip::ApplyClipsToAudio(composition, audio);
  audioReader = AudioReader::Make(audio, outputConfig);
  audioReader->seek(currentProgress);
  empty = audio->getTracks().empty();
}

void PAGAudioReader::updateClock(bool addChangedTimeToSampleTime) {
  if (lastTime == 0) {
    lastSampleTime = 0;
    lastTime = tgfx::Clock::Now();
  }
  int64_t sysTime = tgfx::Clock::Now();
  if (sysTime > lastTime && addChangedTimeToSampleTime) {
    lastSampleTime += (sysTime - lastTime);
  } else {
    lastSampleTime = INT64_MAX;
  }
  lastTime = sysTime;
}

void PAGAudioReader::syncAudio(int64_t sampleTime) {
  if (lastSampleTime == INT64_MAX) {
    lastSampleTime = sampleTime;
    lastTime = tgfx::Clock::Now();
  } else {
    updateClock(true);
  }
  int64_t time = sampleTime - lastSampleTime;
  if (time < -TimeLagNeedToUpdate) {
    lastTime = tgfx::Clock::Now();
    lastSampleTime = sampleTime;
  } else if (time > TimeAheadTolerance) {
    std::this_thread::sleep_for(std::chrono::microseconds(time - TimeAheadTolerance));
  }
}

void PAGAudioReader::onRead() {
  if (empty || !isRunning || !isPlaying) {
    return;
  }
  auto sample = readNextSample();
  if (sample != nullptr) {
    syncAudio(sample->time);
    currentProgress = sample->time;
    Q_EMIT audioTimeChanged(sample->time);
    sendData(std::move(sample));
  } else {
    qDebug() << "Data is empty";
  }
  Q_EMIT read();
}

}  // namespace pag
