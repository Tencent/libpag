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
#include "PAGAudioRender.h"
#include "pag/pag.h"
#include "reader/AudioReader.h"

namespace pag {

class PAGAudioReader : public QThread {
  Q_OBJECT
 public:
  static std::shared_ptr<PAGAudioReader> Make(int sampleRate = 44100, int sampleCount = 1024,
                                              int channels = 2, float volume = 1.0f);

  PAGAudioReader(std::shared_ptr<AudioOutputConfig> outputConfig, float volume);
  ~PAGAudioReader() override;

  bool isEmpty() const;
  void setComposition(std::shared_ptr<PAGComposition> newComposition);
  void setAudioRender(std::shared_ptr<PAGAudioRender> audioRender);
  std::shared_ptr<PAGAudioSample> readNextSample();

  Q_SIGNAL void sendData(std::shared_ptr<PAGAudioSample>);
  Q_SIGNAL void audioTimeChanged(int64_t time);

  Q_SLOT void onPAGProgressChanged(int64_t progress);
  Q_SLOT void onSetIsPlaying(bool isPlaying);

 protected:
  void run() override;

 private:
  void updateAudioTrack();
  void updateClock(bool addChangedTimeToSampleTime);
  void syncAudio(int64_t sampleTime);

  Q_SIGNAL void read();

  Q_SLOT void onRead();

  bool empty = true;
  std::atomic_bool isRunning = false;
  std::atomic_bool isPlaying = false;
  float volume = 1.0f;
  int64_t lastTime = 0;
  int64_t lastSampleTime = INT64_MAX;
  int64_t currentProgress = 0;
  std::shared_ptr<AudioReader> audioReader = nullptr;
  std::shared_ptr<PAGAudioRender> audioRender = nullptr;
  std::shared_ptr<PAGComposition> composition = nullptr;
  std::shared_ptr<AudioOutputConfig> outputConfig = nullptr;
};

}  // namespace pag
