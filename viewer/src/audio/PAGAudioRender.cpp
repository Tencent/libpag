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

void PAGAudioRender::flush() {
  Q_EMIT flushSignal();
}

void PAGAudioRender::setAudioVolume(float volume) {
  Q_EMIT volumeChangeSignal(volume);
}

bool PAGAudioRender::write(std::shared_ptr<PAGAudioSample> data) {
  if (data == nullptr || data->data == nullptr || data->length == 0 || !isPlaying) {
    return false;
  }
  while (isPlaying) {
    int64_t delta = audioOutput->bytesFree() - data->length;
    if (delta < 0) {
      int64_t time = Utils::SampleLengthToTime(-delta, sampleRate, channels);
      std::this_thread::sleep_for(std::chrono::microseconds(time));
    } else {
      break;
    }
  }
  Q_EMIT writeData(data->data, data->length);
  return true;
}

int64_t PAGAudioRender::getAudioLatency() const {
  return Utils::SampleLengthToTime(usedBufferSize, sampleRate, channels);
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
}

void PAGAudioRender::init() {
  audioOutput = std::make_shared<QAudioSink>(QMediaDevices::defaultAudioOutput(), format);
  audioOutput->setBufferSize(DefaultBufferSize);
  audioDevice = audioOutput->start();
  isPlaying = true;
}

void PAGAudioRender::onFlush() {
  audioOutput->reset();
  audioDevice = audioOutput->start();
}

void PAGAudioRender::onVolumeChange(float volume) {
  audioVolume = volume;
  audioOutput->setVolume(volume);
}

void PAGAudioRender::onWriteData(const uint8_t* data, int64_t length) {
  if (!isPlaying) {
    return;
  }
  if (audioOutput->bytesFree() >= length) {
    audioDevice->write(reinterpret_cast<const char*>(data), length);
  }
  usedBufferSize = audioOutput->bufferSize() - audioOutput->bytesFree();
}

}  // namespace pag