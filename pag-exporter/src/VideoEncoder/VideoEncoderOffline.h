/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#ifndef VIDEOENCODEROFFLINE_H
#define VIDEOENCODEROFFLINE_H
#include "VideoEncoder.h"
//#include "VideoEncoderX264.h"

typedef struct FrameInfo {
  FrameType frameType;
  int64_t timeStamp;
  int frameSize;
} FrameInfo;

typedef struct OffLineEncodeParam {
  //std::string uniqueId;
  int width = 0;
  int height = 0;
  double frameRate = 24.0;
  int hasAlpha = 0;
  int maxKeyFrameInterval = 60;
  int quality = 80;
} OffLineEncodeParam;


class VideoEncoderOffline : public VideoEncoder {
public:
  VideoEncoderOffline() {};
  ~VideoEncoderOffline() override;
  bool open(int width, int height, double frameRate, bool hasAlpha, int maxKeyFrameInterval = 0, int quality = 0) override;
  int encodeFrame(uint8_t* data[], int stride[], uint8_t** pOutStream,
                  FrameType* pFrameType, int64_t* pOutTimeStamp) override;
  int encodeHeaders(uint8_t* header[], int headerSize[]) override;
  void getInputFrameBuf(uint8_t* data[], int stride[]) override;

private:
  OffLineEncodeParam param;
  std::string rootPath;

  uint8_t* data[4];
  int stride[4];
  uint8_t header0[1024] = { 0 };
  uint8_t header1[1024] = { 0 };
  uint8_t* h264Buf = nullptr;
  //int headerSize[16] = {0};

  int frames = 0;
  int outFrames = 0;

  //VideoEncoderX264* encoderX264 = nullptr;
};
#endif //VIDEOENCODEROFFLINE_H
