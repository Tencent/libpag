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
#include "VideoEncoderX264.h"

#ifdef _WIN32
#include <io.h>       /* _setmode() */
#include <fcntl.h>    /* _O_BINARY */
#endif

#include <stdint.h>
#include <stdio.h>
#include <x264.h>

static float ConvertQuality2Quant(int quality) {
  const float qpMax = 43;
  const float qpMin = 18;
  if (quality < 0) {
    quality = 0;
  } else if (quality > 100) {
    quality = 100;
  }
  return qpMin + (100 - quality) * 0.01 * (qpMax - qpMin);
}

static int GetMaxRefFramesByLevel(int level, int width, int height) {
  int maxMBs = 36864;
  int maxDPB = 69120;

  switch (level) {
    case 30: // level 3
      maxMBs = 1620;
      maxDPB = 3037;
      break;
    case 31: // level 3.1
      maxMBs = 3600;
      maxDPB = 6750;
      break;
    case 32: // level 3.2
      maxMBs = 5120;
      maxDPB = 7680;
      break;
    case 40: // level 4
      maxMBs = 8192;
      maxDPB = 12288;
      break;
    case 41: // level 4.1
      maxMBs = 8192;
      maxDPB = 12288;
      break;
    case 42: // level 4.2
      maxMBs = 8704;
      maxDPB = 13056;
      break;
    case 50: // level 5
      maxMBs = 22080;
      maxDPB = 41400;
      break;
    case 51: // level 5.1
      maxMBs = 36864;
      maxDPB = 69120;
      break;
    default:
      if (level < 0 || level > 51) {
        maxMBs = 36864;
        maxDPB = 69120;
      } else {
        printf("ERROR! level(%d.%d) < 3.0\n", level / 10, level % 10);
        return 1;
      }
      break;
  }

  int mbsWidth = (width + 15) / 16;
  int mbsHeight = (height + 15) / 16;
  int mbs = mbsWidth * mbsHeight;

  if (mbs > maxMBs) {
    printf("ERROR! frame MB size(%dx%d=%d) > level limit(%d)", mbsWidth, mbsHeight, mbsWidth * mbsHeight, maxMBs);
    return 1;
  }

  for (int refFrames = 16; refFrames >= 1; refFrames--) {
    if (refFrames * mbs < maxDPB) {
      return refFrames;
    }
  }

  return 1;
}

bool VideoEncoderX264::open(int width, int height, double frameRate, bool hasAlpha, int maxKeyFrameInterval, int quality) {
  this->width = width;
  this->height = height;
  this->frameRate = frameRate;

#ifdef WIN32
  const char* preset = "faster";
#else
  const char* preset = "medium";
#endif

  /* Get default params for preset/tuning */
  if (x264_param_default_preset(&param, preset, nullptr) < 0) {
    return false;
  }

  /* Configure non-default params */
  param.i_width = width;
  param.i_height = height;
  param.i_bitdepth = 8;
  param.i_csp = X264_CSP_I420;
  param.b_vfr_input = 0;
  param.b_repeat_headers = 0;
  param.b_annexb = 1;
  param.rc.i_lookahead = 40;

  if (maxKeyFrameInterval > 0) {
    param.i_keyint_max = maxKeyFrameInterval;
    if (param.i_keyint_min > param.i_keyint_max) {
      param.i_keyint_min = param.i_keyint_max;
    }
  }

  if (quality > 0) {
    param.rc.f_rf_constant = ConvertQuality2Quant(quality);
  }

  // 这些参数控制每帧仅一个slice
  param.i_slice_count = 1;
  param.i_threads = 4;
  param.i_lookahead_threads = 4;

  param.i_level_idc = 40;
  param.i_frame_reference = GetMaxRefFramesByLevel(param.i_level_idc, width, height);

  param.i_fps_num = frameRate * param.i_fps_den;
  param.rc.i_rc_method = X264_RC_CRF;
  param.rc.f_rf_constant_max = param.rc.f_rf_constant + 4;
  param.rc.f_rate_tolerance = 3.0;
  param.rc.i_bitrate = (int)(1.0 * 1024 * (width * height) / (1280 * 720));
  if (hasAlpha) {
    param.rc.i_bitrate = param.rc.i_bitrate * 2/3;
  }
  param.rc.i_vbv_max_bitrate = param.rc.i_bitrate * 2;
  param.rc.i_vbv_buffer_size = param.rc.i_bitrate;

  param.vui.b_fullrange = 0;
  param.vui.i_colmatrix = 6; // "matrix_coefficients" 见H.264标准.
  param.vui.i_colorprim = 6; // "colour_primaries" 见H.264标准 170M(1999), bt601

  /* Apply profile restrictions. */
  if (x264_param_apply_profile(&param, "main") < 0) {
    return false;
  }

  if (x264_picture_alloc(&pic, param.i_csp, param.i_width, param.i_height) < 0) {
    return false;
  }

  // 清空YUV
  memset(pic.img.plane[0], 128, pic.img.i_stride[0] * param.i_height >> 0);
  memset(pic.img.plane[1], 128, pic.img.i_stride[1] * param.i_height >> 1);
  memset(pic.img.plane[2], 128, pic.img.i_stride[2] * param.i_height >> 1);

  h = x264_encoder_open(&param);
  if (h == NULL) {
    return false;
  }

  return true;
}

int VideoEncoderX264::encodeHeaders(uint8_t* header[], int headerSize[]) {
  x264_encoder_headers(h, &nal, &i_nal);

  for (int i = 0; i < i_nal; i++) {
    header[i] = nal[i].p_payload;
    headerSize[i] = nal[i].i_payload;
  }

  return i_nal;
}

void VideoEncoderX264::getInputFrameBuf(uint8_t* data[], int stride[]) {
  data[0] = pic.img.plane[0];
  data[1] = pic.img.plane[1];
  data[2] = pic.img.plane[2];
  stride[0] = pic.img.i_stride[0];
  stride[1] = pic.img.i_stride[1];
  stride[2] = pic.img.i_stride[2];
}

int VideoEncoderX264::encodeFrame(uint8_t* data[], int stride[], uint8_t** pOutStream,
                                  FrameType* pFrameType, int64_t* pOutTimeStamp) {
  int size = 0;

  if (data[0] != nullptr) {
    pic.i_pts = i_frame++;
    pic.i_type =
        *pFrameType == FRAME_TYPE_I ? X264_TYPE_IDR : (*pFrameType == FRAME_TYPE_P ? X264_TYPE_P : X264_TYPE_AUTO);

    size = x264_encoder_encode(h, &nal, &i_nal, &pic, &pic_out);
  } else {
    // Flush delayed frames
    do {
      int count = x264_encoder_delayed_frames(h);
      if (count > 0) {
        size = x264_encoder_encode(h, &nal, &i_nal, NULL, &pic_out);
      }
      if (count <= 0 || size > 0) {
        break;
      }
    } while (1);
  }

  if (size > 0) {
    *pOutStream = nal->p_payload;
    *pFrameType = pic_out.b_keyframe ? FRAME_TYPE_I : FRAME_TYPE_P;
    *pOutTimeStamp = pic_out.i_pts;
    return size;
  } else {
    *pOutTimeStamp = 0;
    return 0;
  }

  return size;
}

VideoEncoderX264::~VideoEncoderX264() {
  if (h != NULL) {
    x264_encoder_close(h);
    h = NULL;
  }
  x264_picture_clean(&pic);
}
#endif
