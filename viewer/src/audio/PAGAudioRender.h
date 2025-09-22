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

#include <QAudioFormat>
#include <QAudioSink>
#include <QThread>
#include "reader/AudioReader.h"

namespace pag {

class PAGAudioRender : public QThread {
  Q_OBJECT
 public:
  static std::shared_ptr<PAGAudioRender> Make(int sampleRate, int channels);

  ~PAGAudioRender() override;
  void flush();
  void setAudioVolume(float volume);
  bool write(std::shared_ptr<PAGAudioSample> data);
  int64_t getAudioLatency() const;

  Q_SLOT void onSetIsPlaying(bool isPlaying);

 private:
  explicit PAGAudioRender(const QAudioFormat& format, int sampleRate, int channels);
  void init();

  Q_SIGNAL void flushSignal();
  Q_SIGNAL void volumeChangeSignal(float volume);
  Q_SIGNAL void writeData(const uint8_t* data, int64_t length);

  Q_SLOT void onFlush();
  Q_SLOT void onVolumeChange(float volume);
  Q_SLOT void onWriteData(const uint8_t* data, int64_t length);

  int channels = 2;
  int sampleRate = 44100;
  float audioVolume = 1.0f;
  std::atomic_bool isPlaying = false;
  int64_t usedBufferSize = 0;
  QAudioFormat format = {};
  QIODevice* audioDevice = nullptr;
  std::shared_ptr<QAudioSink> audioOutput = nullptr;
};

}  // namespace pag
