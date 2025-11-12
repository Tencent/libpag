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

#include <memory>

namespace exporter {

#define SIZE_ALIGN(x) (((x) + 1) & ~0x01)

enum class FrameType {
  FRAME_TYPE_AUTO = -1,
  FRAME_TYPE_P = 0,
  FRAME_TYPE_I = 1,
  FRAME_TYPE_B = 2,
};

class VideoEncoder {
 public:
  static std::unique_ptr<VideoEncoder> MakeVideoEncoder(bool hardwareEncode = false);
  virtual ~VideoEncoder() = default;
  virtual bool open(int width, int height, double frameRate, bool hasAlpha, int maxKeyFrameInterval,
                    int quality) = 0;
  virtual void close() = 0;
  virtual void encodeFrame(uint8_t* data[], int stride[], FrameType frameType) = 0;
  virtual int encodeHeaders(uint8_t* header[], int headerSize[]) = 0;
  virtual void getInputFrameBuf(uint8_t* data[], int stride[]) = 0;
  virtual int getEncodedFrame(bool wait, uint8_t** outData, FrameType* outFrameType,
                              int64_t* outFrameIndex) = 0;

 protected:
  int width = 0;
  int height = 0;
  double frameRate = 24.0;
};

class PAGEncoder {
 public:
  explicit PAGEncoder(bool hardwareEncode = false);
  bool init(int width, int height, double frameRate, bool hasAlpha, int maxKeyFrameInterval = 0,
            int quality = 0);
  void getAlphaStartXY(int32_t* pAlphaStartX, int32_t* pAlphaStartY);
  void encodeRGBA(uint8_t* data, int dataStride, FrameType frameType);
  int encodeHeaders(uint8_t* header[], int headerSize[]);
  int getEncodedData(uint8_t** outData, FrameType* outFrameType, int64_t* outFrameIndex);
  void close();

 private:
  const int paddingX = 4;
  const int paddingY = 4;
  int inputWidth = 0;
  int inputHeight = 0;
  int internalWidth = 0;
  int internalHeight = 0;
  bool hasAlpha = false;
  int32_t alphaStartX = 0;
  int32_t alphaStartY = 0;
  std::unique_ptr<VideoEncoder> encoder = nullptr;
};

}  // namespace exporter