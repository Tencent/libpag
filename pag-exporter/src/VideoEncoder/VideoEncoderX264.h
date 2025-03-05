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
#if 0
#ifndef VIDEOENCODERX264_H
#define VIDEOENCODERX264_H
#include "VideoEncoder.h"
#include "x264.h"

class VideoEncoderX264 : public VideoEncoder {
public:
  VideoEncoderX264() {};
  ~VideoEncoderX264() override;
  bool open(int width, int height, double frameRate, bool hasAlpha, int maxKeyFrameInterval = 0, int quality = 0) override;
  int encodeFrame(uint8_t* data[], int stride[], uint8_t** pOutStream,
                  FrameType* pFrameType, int64_t* pOutTimeStamp) override;
  int encodeHeaders(uint8_t* header[], int headerSize[]) override;
  void getInputFrameBuf(uint8_t* data[], int stride[]) override;

private:
  x264_param_t param;
  x264_picture_t pic;
  x264_picture_t pic_out;
  x264_t* h;
  int i_frame = 0;
  x264_nal_t* nal;
  int i_nal;
};
#endif //VIDEOENCODERX264_H
#endif