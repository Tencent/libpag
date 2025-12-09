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

#include <QProcess>
#include <array>
#include <string>
#include <vector>
#include "VideoEncoder.h"

namespace exporter {

struct FrameInfo {
  FrameType frameType = FrameType::FRAME_TYPE_AUTO;
  int frameSize = 0;
  int64_t timeStamp = 0;
};

struct OfflineEncodeParam {
  int width = 0;
  int height = 0;
  int hasAlpha = 0;
  int quality = 80;
  int maxKeyFrameInterval = 60;
  double frameRate = 24.0;
};

class OfflineVideoEncoder : public VideoEncoder {
 public:
  ~OfflineVideoEncoder() override;
  bool open(int width, int height, double frameRate, bool hasAlpha, int maxKeyFrameInterval,
            int quality) override;
  void close() override;
  void getInputFrameBuf(uint8_t* data[], int stride[]) override;
  int encodeHeaders(uint8_t* header[], int headerSize[]) override;
  void encodeFrame(uint8_t* data[], int stride[], FrameType frameType) override;
  int getEncodedFrame(bool wait, uint8_t** outData, FrameType* outFrameType,
                      int64_t* outFrameIndex) override;

 private:
  void exit();
  static bool readEndParam(bool& hasEnd, bool& earlyExit, const std::string& filePath);
  static bool writeEndParam(bool hasEnd, bool earlyExit, const std::string& filePath);
  static bool readFrameInfo(FrameInfo& frameInfo, const std::string& filePath);
  static bool writeFrameInfo(const FrameInfo& frameInfo, const std::string& filePath);

  int frames = 0;
  int outFrames = 0;
  OfflineEncodeParam param = {};
  std::string rootPath = "";
  std::array<int, 4> stride = {0};
  std::array<uint8_t, 1024> header[2] = {{0}, {0}};
  std::vector<uint8_t> data[3] = {};
  std::vector<uint8_t> h264Buf = {};
  std::unique_ptr<QProcess> process = nullptr;
};
}  // namespace exporter
