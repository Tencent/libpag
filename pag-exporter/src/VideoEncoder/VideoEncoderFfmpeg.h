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
#ifndef VIDEOENCODERFFMPEG_H
#define VIDEOENCODERFFMPEG_H
#include "VideoEncoder.h"

#ifdef __cplusplus
extern "C" {
#endif
#define __STDC_CONSTANT_MACROS
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
}
#endif

class VideoEncoderFfmpeg : public VideoEncoder {
public:
  static bool TestBeEnable() {
#ifdef WIN32
    return false;
#else
    return false; // 此版本ffmpeg库不支持硬编，先以软编发包
#endif
  };
  VideoEncoderFfmpeg() {};
  ~VideoEncoderFfmpeg() override;
  bool open(int width, int height, double frameRate, bool hasAlpha, int maxKeyFrameInterval = 0, int quality = 0) override;
  int encodeFrame(uint8_t* data[], int stride[], uint8_t** pOutStream,
                  FrameType* pFrameType, int64_t* pOutTimeStamp) override;
  int encodeHeaders(uint8_t* header[], int headerSize[]) override;
  void getInputFrameBuf(uint8_t* data[], int stride[]) override;

private:
  AVCodecContext* c = nullptr;
  AVFrame* frame = nullptr;
  AVFrame* allocPicture(enum AVPixelFormat pix_fmt, int width, int height);

  void checkAndResizeOutBuf(int newSize);
  uint8_t* outStreamBuf = nullptr;
  int outStreamBufSize = 0;
};
#endif //VIDEOENCODERFFMPEG_H
#endif