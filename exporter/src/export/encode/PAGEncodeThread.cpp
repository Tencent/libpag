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

#include "PAGEncodeThread.h"
#include <QGuiApplication.h>

namespace exporter {
PAGEncodeThread::PAGEncodeThread(QObject* parent) : QThread(parent) {
  this->moveToThread(this);
  connect(this, &PAGEncodeThread::getEncodeFrameSignal, this, &PAGEncodeThread::getEncodeFrame,
          Qt::QueuedConnection);
  connect(this, &PAGEncodeThread::encodeHeadersSignal, this,
          &PAGEncodeThread::encodeHeadersInternal, Qt::QueuedConnection);
  connect(this, &PAGEncodeThread::encodeFrameSignal, this, &PAGEncodeThread::encodeFrameInternal,
          Qt::QueuedConnection);
  start();
}

PAGEncodeThread::~PAGEncodeThread() {
  encoder->close();
  quit();
  wait();
}

bool PAGEncodeThread::isValid() const {
  return valid;
}

void PAGEncodeThread::close() {
  inputFinished = true;
  if (encoder != nullptr) {
    encoder->close();
  }
}

void PAGEncodeThread::setPAGEncoder(std::unique_ptr<PAGEncoder> encoder) {
  this->encoder = std::move(encoder);
}

void PAGEncodeThread::setEncodeFrameCallback(const EncodeFrameCallback& callback) {
  encodeFrameCallback = callback;
}

void PAGEncodeThread::setEncodeHeaderCallback(const EncodeHeaderCallback& callback) {
  encodeHeaderCallback = callback;
}

void PAGEncodeThread::getAlphaStartXY(int32_t* pAlphaStartX, int32_t* pAlphaStartY) {
  *pAlphaStartX = alphaStartX;
  *pAlphaStartY = alphaStartY;
}

void PAGEncodeThread::encodeHeaders() {
  Q_EMIT encodeHeadersSignal();
}

void PAGEncodeThread::encodeFrame(const std::unique_ptr<pag::ByteData>& data, FrameType frameType,
                                  int stride) {
  auto newData = pag::ByteData::MakeCopy(data->data(), data->length());
  Q_EMIT encodeFrameSignal(std::move(newData), frameType, stride);
}

void PAGEncodeThread::run() {
  QThread::exec();
}

void PAGEncodeThread::getEncodeFrame() {
  if (encoder == nullptr) {
    return;
  }
  FrameType frameType = FrameType::FRAME_TYPE_AUTO;
  uint8_t* data = nullptr;
  int64_t index = -1;
  auto size = encoder->getEncodedData(&data, &frameType, &index);
  if (size > 0 && data != nullptr) {
    auto newData = pag::ByteData::MakeCopy(data, size);
    hasEncodedNum++;
    if (encodeFrameCallback != nullptr) {
      encodeFrameCallback(std::move(newData), frameType, index);
    }
  } else if (size < 0) {
    valid = false;
  }
  if ((hasEncodedNum < needEncodeNum) || !inputFinished) {
    std::this_thread::sleep_for(std::chrono::microseconds(1000));
    Q_EMIT getEncodeFrameSignal();
  }
}

void PAGEncodeThread::encodeHeadersInternal() {
  if (encoder == nullptr) {
    return;
  }
  encoder->getAlphaStartXY(&alphaStartX, &alphaStartY);
  uint8_t* networkAbstractionLayerUnits[16] = {nullptr};
  int size[16] = {0};
  auto count = encoder->encodeHeaders(networkAbstractionLayerUnits, size);
  if (count >= 2) {
    auto sequenceParameterSet = new uint8_t[size[0]];
    auto pictureParameterSet = new uint8_t[size[1]];
    memcpy(sequenceParameterSet, networkAbstractionLayerUnits[0], size[0]);
    memcpy(pictureParameterSet, networkAbstractionLayerUnits[1], size[1]);
    auto spsBytes = pag::ByteData::MakeAdopted(sequenceParameterSet, size[0]).release();
    auto ppsBytes = pag::ByteData::MakeAdopted(pictureParameterSet, size[1]).release();

    std::vector<pag::ByteData*> headers = {spsBytes, ppsBytes};
    if (encodeHeaderCallback != nullptr) {
      encodeHeaderCallback(headers);
    }
  }
}

void PAGEncodeThread::encodeFrameInternal(std::shared_ptr<pag::ByteData> data, FrameType frameType,
                                          int stride) {
  if (encoder == nullptr) {
    return;
  }
  needEncodeNum++;
  encoder->encodeRGBA(data->data(), stride, frameType);
  if (needEncodeNum == 1) {
    Q_EMIT getEncodeFrameSignal();
  }
}

}  // namespace exporter
