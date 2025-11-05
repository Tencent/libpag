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

#include "PAGAudioRender.h"
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDebug>
#include <QMediaDevices>
#include "utils/AudioUtils.h"

namespace pag {

constexpr int64_t DefaultBufferSize = 8192;

std::shared_ptr<PAGAudioRender> PAGAudioRender::Make(int sampleRate, int channels) {
  auto format = QAudioFormat();
  format.setSampleRate(sampleRate);
  format.setChannelCount(channels);
  format.setSampleFormat(QAudioFormat::SampleFormat::Int16);
  QAudioDevice device(QMediaDevices::defaultAudioOutput());
  if (!device.isFormatSupported(format)) {
    qDebug() << "Raw audio format not supported by backend, cannot play audio.";
    return nullptr;
  }
  return std::shared_ptr<PAGAudioRender>(new PAGAudioRender(format, sampleRate, channels));
}

PAGAudioRender::~PAGAudioRender() {
  quit();
  wait();
}

void PAGAudioRender::setAudioVolume(float volume) {
  Q_EMIT volumeChangeSignal(volume);
}

bool PAGAudioRender::write(std::shared_ptr<PAGAudioSample> data) {
  if (data == nullptr || data->data == nullptr || data->length == 0 || !isPlaying) {
    return false;
  }
  int64_t delta = audioOutput->bytesFree() - data->length;
  if (delta < 0) {
    int64_t waitTime = Utils::SampleLengthToTime(-delta, sampleRate, channels);
    std::this_thread::sleep_for(std::chrono::microseconds(waitTime));
  }
  Q_EMIT writeData(data->data);
  return true;
}

void PAGAudioRender::onSetIsPlaying(bool isPlaying) {
  this->isPlaying = isPlaying;
  if (isPlaying) {
    audioOutput->resume();
  } else {
    audioOutput->suspend();
  }
}

PAGAudioRender::PAGAudioRender(const QAudioFormat& format, int sampleRate, int channels)
    : channels(channels), sampleRate(sampleRate), format(format) {
  this->moveToThread(this);
  connect(this, &PAGAudioRender::volumeChangeSignal, this, &PAGAudioRender::onVolumeChange);
  connect(this, &PAGAudioRender::writeData, this, &PAGAudioRender::onWriteData);
  start();
  init();
  isPlaying = true;
}

void PAGAudioRender::init() {
  if (audioOutput) {
    audioOutput->stop();
    audioOutput->reset();
    audioOutput = nullptr;
  }
  audioOutput = std::make_shared<QAudioSink>(QMediaDevices::defaultAudioOutput(), format);
  audioOutput->setBufferSize(DefaultBufferSize);
  audioDevice = audioOutput->start();
}

void PAGAudioRender::onVolumeChange(float volume) {
  audioVolume = volume;
  audioOutput->setVolume(volume);
}

void PAGAudioRender::onWriteData(std::shared_ptr<ByteData> data) {
  if (!isPlaying) {
    return;
  }
  if (audioOutput->bytesFree() >= static_cast<int64_t>(data->length())) {
    audioDevice->write(reinterpret_cast<const char*>(data->data()), data->length());
  }
}

}  // namespace pag