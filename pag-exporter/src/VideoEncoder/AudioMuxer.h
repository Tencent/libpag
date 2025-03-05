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
#ifndef AUDIOMUXER_H
#define AUDIOMUXER_H
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

#include "src/exports/PAGDataTypes.h"

// a wrapper around a single output AVStream
typedef struct OutputStream {
  AVStream* st;
  AVCodecContext* enc;

  /* pts of the next frame that will be generated */
  int64_t next_pts;
  int samples_count;

  AVFrame* internalFrame;
  AVFrame* tmp_frame;

  struct SwrContext* swr_ctx;
} OutputStream;

class AudioMuxer {
public:
  AudioMuxer() {}

  ~AudioMuxer();
  bool init(std::string& outputPath, int channel, int sampleRate);
  void close();
  int putData(int16_t* data, int numSamples);
  std::unique_ptr<pag::ByteData> getByteData();

private:
  std::string filename;
  int channel = 2;
  int sampleRate = 44100;
  OutputStream audio_st = {0};
  AVOutputFormat* fmt = nullptr;
  AVFormatContext* oc = nullptr;
  AVCodec* audio_codec = nullptr;
};

#endif //AUDIOMUXER_H
