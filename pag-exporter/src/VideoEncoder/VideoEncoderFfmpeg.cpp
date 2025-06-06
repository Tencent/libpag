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
#include "VideoEncoderFfmpeg.h"

void VideoEncoderFfmpeg::checkAndResizeOutBuf(int newSize) {
  if (outStreamBufSize < newSize) {
    outStreamBufSize = newSize * 3/2 + 1024;
    if (outStreamBuf != nullptr) {
      delete outStreamBuf;
    }
    outStreamBuf = new uint8_t[outStreamBufSize];
  }
}

AVFrame* VideoEncoderFfmpeg::allocPicture(enum AVPixelFormat pix_fmt, int width, int height) {
  AVFrame* picture;
  int ret;

  picture = av_frame_alloc();
  if (!picture) {
    return nullptr;
  }

  picture->format = pix_fmt;
  picture->width = width;
  picture->height = height;
  picture->pts = 0;

  /* allocate the buffers for the frame data */
  ret = av_frame_get_buffer(picture, 32);
  if (ret < 0) {
    printf("Could not allocate frame data.\n");
    return nullptr;
  }

  return picture;
}

bool VideoEncoderFfmpeg::open(int width, int height, double frameRate, bool hasAlpha, int maxKeyFrameInterval, int quality) {
  this->width = width;
  this->height = height;
  this->frameRate = frameRate;

  const char* typestr = "h264_videotoolbox";
  auto codec = avcodec_find_encoder_by_name(typestr);
  if (codec == nullptr) {
    printf("Could not find encoder for '%s'\n", typestr);
    return false;
  }

  c = avcodec_alloc_context3(codec);
  if (!c) {
    printf("Could not alloc an encoding context\n");
    return false;
  }

  c->codec_id = AV_CODEC_ID_H264;
  c->bit_rate = 8000000;
  c->width = width;
  c->height = height;
  c->time_base.num = 1;
  c->time_base.den = 24;
  c->gop_size = 60;
  c->pix_fmt = AV_PIX_FMT_YUV420P;
  c->flags = c->flags | AV_CODEC_FLAG_GLOBAL_HEADER;

  AVDictionary* opt_arg = nullptr;
  AVDictionary* pVoid = nullptr;

  av_dict_copy(&pVoid, opt_arg, 0);
  auto ret = avcodec_open2(c, codec, &pVoid);
  av_dict_free(&pVoid);
  if (ret < 0) {
    printf("Could not open video codec!!!\n");
    return false;
  }

  frame = allocPicture(c->pix_fmt, c->width, c->height);
  if (frame == nullptr) {
    printf("Could not allocate video frame\n");
    return -1;
  }

  return true;
}

void VideoEncoderFfmpeg::getInputFrameBuf(uint8_t* data[], int stride[]) {
  data[0] = frame->data[0];
  data[1] = frame->data[1];
  data[2] = frame->data[2];
  stride[0] = frame->linesize[0];
  stride[1] = frame->linesize[1];
  stride[2] = frame->linesize[2];
}

int VideoEncoderFfmpeg::encodeFrame(uint8_t* data[], int stride[], uint8_t** pOutStream,
                                    FrameType* pFrameType, int64_t* pOutTimeStamp) {
  int size = 0;
  AVPacket pkt = {nullptr};
  av_init_packet(&pkt);
  int ret = avcodec_send_frame(c, data[0] != nullptr ? frame : nullptr);
  if (data[0] && ret < 0) {
    printf("Error encoding video frame!!!\n");
    return size;
  }
  if (data[0] != nullptr) {
    frame->pts++;
  }

  int endStatus = 0;
  int outRet = 0;
  while (outRet >= 0) {
    outRet = avcodec_receive_packet(c, &pkt);
    if (outRet == AVERROR_EOF) {
      endStatus = 1;
      break;
    } else if (outRet == 0) {
      break;
    } else if (outRet != AVERROR(EAGAIN)) {
      printf("Error while avcodec_receive_packet frame!!!\n");
      break;
    }
  }
  if (pkt.size > 0) {
    checkAndResizeOutBuf(pkt.size);
    uint8_t* p = pkt.data;
    size = pkt.size;
    if (p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01 && p[4] == 0x06) {
      for (int i = 5; i < size - 5; i++) {
        if (p[i+0] == 0x00 && p[i+1] == 0x00 && p[i+2] == 0x00 && p[i+3] == 0x01) {
          p += i;
          size -= i;
          break;
        }
      }
    }
    if (p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01) {
      if (p[4] == 0x25) {
        //p[4] = 0x65;
      } else if (p[4] == 0x21) {
        //p[4] = 0x41;
      }
    }
    memcpy(outStreamBuf, p, size);
    *pOutStream = outStreamBuf;
    *pFrameType = ((pkt.flags&AV_PKT_FLAG_KEY) == AV_PKT_FLAG_KEY) ? FRAME_TYPE_I : FRAME_TYPE_P;
    *pOutTimeStamp = pkt.pts;
  }
  av_packet_unref(&pkt);
  return size;
}

static int FindStartCode(uint8_t* data, int size) {
  for (int index = 0; index < size - 4; index++) {
    if (data[index+0] == 0x00 && data[index+1] == 0x00 && data[index+2] == 0x00 && data[index+3] == 0x01) {
      return index;
    }
  }
  return -1;
}

static int FindNalData(uint8_t* data, int size, int* pStartIndex) {
  *pStartIndex = 0;
  int startIndex = FindStartCode(data, size);
  if (startIndex < 0) {
    return 0;
  }
  *pStartIndex = startIndex;

  int nextStartIndex = FindStartCode(data + startIndex + 4, size - startIndex - 4);
  if (nextStartIndex < 0) {
    return size - startIndex;
  } else {
    return nextStartIndex + 4;
  }
}

int VideoEncoderFfmpeg::encodeHeaders(uint8_t* header[], int headerSize[]) {
  uint8_t* data = c->extradata;
  int size = c->extradata_size;
  header[0] = nullptr;
  header[1] = nullptr;
  headerSize[0] = 0;
  headerSize[1] = 0;
  for (int index = 0; index < size;) {
    int startIndex = 0;
    int len = FindNalData(data + index, size - index, &startIndex);
    if (len <= 0) {
      break;
    }
    uint8_t byte = data[index + startIndex + 4];
    if (byte == 0x67 || byte == 0x27) {
      //data[index + startIndex + 4] = 0x67;
      header[0] = data + index + startIndex;
      headerSize[0] = len;
    } else if (byte == 0x68 || byte == 0x28) {
      //data[index + startIndex + 4] = 0x68;
      header[1] = data + index + startIndex;
      headerSize[1] = len;
    }
    index += startIndex + len;
  }
  return (header[0] != nullptr && header[1] != nullptr) ? 2 : 0;
}

VideoEncoderFfmpeg::~VideoEncoderFfmpeg() {
  avcodec_close(c);
  av_frame_free(&frame);
  if (outStreamBuf != nullptr) {
    delete outStreamBuf;
  }
}

int main_gpu2() {
  FILE* fp1 = fopen("/Users/chenxinxing/pag/TAVKit-linux/Resources/test.yuv", "rb");
  FILE* fp2 = fopen("/Users/chenxinxing/data/xx.264", "wb");
  int sumSize = 0;
  int width = 1280;
  int height = 720;
  uint8_t* data[4] = {nullptr};
  int stride[4] = {width, width/2, width/2};
  data[0] = new uint8_t[width*height * 3/2];
  data[1] = data[0] + width*height;
  data[2] = data[0] + width*height * 5/4;

  auto enc = new VideoEncoderFfmpeg();
  enc->open(width, height, 0, 24.0);
  for (int index = 0; index < 100; index++) {
    auto len = fread(data[0], 1, width*height*3/2, fp1);
    if (len < width*height*3/2) {
      break;
    }
    uint8_t* pOutStream = nullptr;
    FrameType frameType = FRAME_TYPE_AUTO;
    int64_t outTimeStamp = 0;
    auto size = enc->encodeFrame(data, stride, &pOutStream, &frameType, &outTimeStamp);
    if (size > 0 && pOutStream != nullptr) {
      fwrite(pOutStream, 1, size, fp2);
      fflush(fp2);
      sumSize += size;
    }
    printf("%d: size=%d sum=%d\n", index, size, sumSize);
  }
  delete enc;
  fclose(fp1);
  fclose(fp2);
  delete data[0];
  return 0;
}
#endif
