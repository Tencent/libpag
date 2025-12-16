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

#include <pag/types.h>
#include <tgfx/core/Buffer.h>
#include <QThread>
#include "VideoEncoder.h"

namespace exporter {

using EncodeHeaderCallback = std::function<void(std::vector<pag::ByteData*> headers)>;
using EncodeFrameCallback =
    std::function<void(std::unique_ptr<pag::ByteData> data, FrameType frameType, int64_t index)>;

class PAGEncodeThread : public QThread {
  Q_OBJECT
 public:
  explicit PAGEncodeThread(QObject* parent = nullptr);
  ~PAGEncodeThread() override;

  bool isValid() const;
  void close();
  void setPAGEncoder(std::unique_ptr<PAGEncoder> encoder);
  void setEncodeFrameCallback(const EncodeFrameCallback& callback);
  void setEncodeHeaderCallback(const EncodeHeaderCallback& callback);
  void getAlphaStartXY(int32_t* pAlphaStartX, int32_t* pAlphaStartY);

  void encodeHeaders();
  void encodeFrame(const std::unique_ptr<pag::ByteData>& data, FrameType frameType, int stride);

 private:
  void run() override;

  Q_SIGNAL void getEncodeFrameSignal();
  Q_SIGNAL void encodeHeadersSignal();
  Q_SIGNAL void encodeFrameSignal(std::shared_ptr<pag::ByteData> data, FrameType frameType,
                                  int stride);

  Q_SLOT void getEncodeFrame();
  Q_SLOT void encodeHeadersInternal();
  Q_SLOT void encodeFrameInternal(std::shared_ptr<pag::ByteData> data, FrameType frameType,
                                  int stride);

  bool valid = true;
  bool inputFinished = false;
  int64_t needEncodeNum = 0;
  int64_t hasEncodedNum = 0;
  int32_t alphaStartX = 0;
  int32_t alphaStartY = 0;
  EncodeHeaderCallback encodeHeaderCallback = nullptr;
  EncodeFrameCallback encodeFrameCallback = nullptr;
  std::unique_ptr<PAGEncoder> encoder = nullptr;
};

}  // namespace exporter