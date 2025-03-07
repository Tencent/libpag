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
#include "VideoEncoderOffline.h"
#include "VideoEncoderX264.h"
#include "src/cJSON/cJSON.h"
#include "src/configparam/ConfigParam.h"
#ifdef _WIN32
#include <windows.h>
#define usleep(x) Sleep(x / 1000)  // 将微秒转换为毫秒
#else
#include <unistd.h>
#endif
#include <filesystem>

#define SLEEP_US 1000
#define SLEEP_COUNT 2000

namespace fs = std::filesystem;

static std::string GetH264EncoderToolsFolder() {
  const auto res=fs::path(GetRoamingPath()) / "H264EncoderTools/";
  return res.string();
}
static std::string GetOfflineFolder() {
  const auto res=fs::path(GetH264EncoderToolsFolder()) / "OffLineFolder/";
  return res.string();
}

static int ReadFileData(const std::string& path, uint8_t data[]) {
  const auto fp = fopen(path.c_str(), "rb");
  if (fp == nullptr) {
    return 0;
  }

  fseek(fp, 0, SEEK_END);
  auto len = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  len = fread(data, 1, len, fp);
  fclose(fp);
  data[len] = '\0';

  return static_cast<int>(len);
}

static bool WriteParamToFile(OffLineEncodeParam* param) {
  char text[1024] = {0};
  snprintf(text, sizeof(text),
           "{ \"Width\": %d, \"Height\": %d, \"FrameRate\": %.2f, \"HasAlpha\": %d, "
           "\"MaxKeyFrameInterval\": %d, \"Quality\": %d }",
           param->width, param->height, param->frameRate, param->hasAlpha,
           param->maxKeyFrameInterval, param->quality);
  auto path = GetOfflineFolder() + "EncoderParam.txt";

  auto fp = fopen(path.c_str(), "wt");
  if (fp == nullptr) {
    return false;
  }
  fprintf(fp, "%s", text);
  fclose(fp);

  return true;
}

static cJSON* ParseJsonFile(std::string path) {
  char text[4 * 1024];
  auto fp = fopen(path.c_str(), "rt");
  if (fp == nullptr) {
    return nullptr;
  }

  fseek(fp, 0, SEEK_END);
  auto len = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (len == 0 || len >= sizeof(text)) {
    printf("ParseJsonFile size error!\n");
    fclose(fp);
    return nullptr;
  }
  len = fread(text, 1, len, fp);
  fclose(fp);
  text[len] = '\0';

  auto root = cJSON_Parse(text);
  return root;
}

template <typename T>
static bool GetIntValue(cJSON* root, std::string str, T& value) {
  auto item = cJSON_GetObjectItem(root, str.c_str());
  if (item != nullptr && item->type == cJSON_Int) {
    value = static_cast<T>(item->valueint);
    return true;
  } else {
    return false;
  }
}

static bool WriteFrameInfo(FrameInfo& frameInfo, std::string path) {
  auto fp = fopen(path.c_str(), "wt");
  if (fp == nullptr) {
    return false;
  }
  fprintf(fp, "{ \"FrameType\": %d, \"TimeStamp\": %.0f, \"FrameSize\": %d }", frameInfo.frameType,
          (float)frameInfo.timeStamp, frameInfo.frameSize);
  fclose(fp);
  return true;
}

static bool ReadFrameInfo(FrameInfo& frameInfo, std::string path) {
  auto root = ParseJsonFile(path);
  if (root == nullptr) {
    return false;
  }

  bool ret = true;
  int frameType = -1;
  ret = ret && GetIntValue(root, "FrameType", frameType);
  ret = ret && GetIntValue(root, "TimeStamp", frameInfo.timeStamp);
  ret = ret && GetIntValue(root, "FrameSize", frameInfo.frameSize);
  frameInfo.frameType = static_cast<FrameType>(frameType);

  return ret;
}

static bool ReadEndParam(bool& hasEnd, bool& bEarlyExit, std::string path) {
  auto root = ParseJsonFile(path);
  if (root == nullptr) {
    return false;
  }

  int end = 0;
  int exit = 0;

  bool ret = true;
  ret = ret && GetIntValue(root, "HasEnd", end);
  ret = ret && GetIntValue(root, "EarlyExit", exit);

  hasEnd = !!end;
  bEarlyExit = !!exit;

  return ret;
}

static bool WriteEndParam(bool hasEnd, bool bEarlyExit, std::string path) {
  auto fp = fopen(path.c_str(), "wt");
  if (fp == nullptr) {
    return false;
  }
  fprintf(fp, "{ \"HasEnd\": %d, \"EarlyExit\": %d }", hasEnd ? 1 : 0, bEarlyExit ? 1 : 0);
  fclose(fp);
  return true;
}

bool VideoEncoderOffline::open(int width, int height, double frameRate, bool hasAlpha,
                               int maxKeyFrameInterval, int quality) {
  this->width = width;
  this->height = height;
  this->frameRate = frameRate;

  h264Buf = new uint8_t[width * height + 1024];

  param.width = width;
  param.height = height;
  param.frameRate = frameRate;
  param.hasAlpha = hasAlpha;
  param.quality = quality;
  param.maxKeyFrameInterval = maxKeyFrameInterval;

  stride[0] = SIZE_ALIGN(width);
  stride[1] = SIZE_ALIGN(stride[0] / 2);
  stride[2] = stride[1];
  data[0] = new uint8_t[stride[0] * (SIZE_ALIGN(height) / 1)];
  data[1] = new uint8_t[stride[1] * (SIZE_ALIGN(height) / 2)];
  data[2] = new uint8_t[stride[2] * (SIZE_ALIGN(height) / 2)];

  // 清空YUV
  memset(data[0], 128, stride[0] * (SIZE_ALIGN(height) >> 0));
  memset(data[1], 128, stride[1] * (SIZE_ALIGN(height) >> 1));
  memset(data[2], 128, stride[2] * (SIZE_ALIGN(height) >> 1));

  rootPath = GetOfflineFolder();
  ReCreateFolder(GetOfflineFolder());
  WriteParamToFile(&param);

#ifdef WIN32
  std::string cmd2 = "\"" + GetH264EncoderToolsFolder() + "H264EncoderTools.exe\" " + GetOfflineFolder();
  WinExec(cmd2.c_str(), SW_HIDE);
#elif defined(__APPLE__) || defined(__MACH__)
  std::string cmd2 =
      "\'" + GetH264EncoderToolsFolder() + "H264EncoderTools\' \'" + GetOfflineFolder() + "\' &";
  system(cmd2.c_str());
#endif
  return true;
}

int VideoEncoderOffline::encodeHeaders(uint8_t* header[], int headerSize[]) {
  for (int i = 0; i < SLEEP_COUNT; i++) {
    int size0 = ReadFileData(rootPath + "Header-0.dat", header0);
    int size1 = ReadFileData(rootPath + "Header-1.dat", header1);
    if (size0 > 0 && size1 > 0) {
      header[0] = header0;
      header[1] = header1;
      headerSize[0] = size0;
      headerSize[1] = size1;
      return 2;
    } else {
      printf("sleep... encodeHeaders=%d\n", i);
      usleep(SLEEP_US);
    }
  }
  return 0;
}

void VideoEncoderOffline::getInputFrameBuf(uint8_t* data[], int stride[]) {
  data[0] = this->data[0];
  data[1] = this->data[1];
  data[2] = this->data[2];
  stride[0] = this->stride[0];
  stride[1] = this->stride[1];
  stride[2] = this->stride[2];
}

static bool WriteYUVPlane(uint8_t* data, int stride, int width, int height, FILE* fp) {
  for (int j = 0; j < height; j++) {
    auto size = fwrite(data + j * stride, 1, width, fp);
    if (static_cast<int>(size) != width) {
      return false;
    }
    fflush(fp);
  }
  return true;
}

static bool WriteYUVData(uint8_t* data[4], int stride[4], int width, int height, std::string path) {
  auto fp = fopen(path.c_str(), "wb");
  if (fp == nullptr) {
    return false;
  }
  bool ret = true;
  ret = ret && WriteYUVPlane(data[0], stride[0], width / 1, height / 1, fp);
  ret = ret && WriteYUVPlane(data[1], stride[1], width / 2, height / 2, fp);
  ret = ret && WriteYUVPlane(data[2], stride[2], width / 2, height / 2, fp);
  fflush(fp);
  fclose(fp);
  return ret;
}

int VideoEncoderOffline::encodeFrame(uint8_t* data[], int stride[], uint8_t** pOutStream,
                                     FrameType* pFrameType, int64_t* pOutTimeStamp) {
  if (data[0] != nullptr) {
    WriteYUVData(data, stride, width, height, rootPath + "YUV-" + std::to_string(frames) + ".yuv");

    FrameInfo frameInfo;
    frameInfo.frameType = *pFrameType;
    frameInfo.timeStamp = frames;
    frameInfo.frameSize = width * height * 3 / 2;

    WriteFrameInfo(frameInfo, rootPath + "InFrameInfo-" + std::to_string(frames) + ".txt");
    frames++;
  } else {
    WriteEndParam(1, 0, rootPath + "InEnd.txt");
  }

  for (int i = 0; i < SLEEP_COUNT; i++) {
    FrameInfo outFrameInfo;
    auto ret = ReadFrameInfo(outFrameInfo,
                             rootPath + "OutFrameInfo-" + std::to_string(outFrames) + ".txt");
    if (ret) {
      int size = ReadFileData(rootPath + "H264-" + std::to_string(outFrames) + ".264", h264Buf);
      if (size > 0 && size == outFrameInfo.frameSize) {
        printf("read frame=%d size = %d outSize = %d\n", outFrames, size, outFrameInfo.frameSize);
        *pOutStream = h264Buf;
        *pFrameType = outFrameInfo.frameType;
        *pOutTimeStamp = outFrameInfo.timeStamp;
        outFrames++;
        return size;
      }
    }

    if (data[0] == nullptr && outFrames < frames) {
      printf("sleep... encodeFrame=%d\n", i);
      usleep(SLEEP_US);
    } else {
      break;
    }
  }

  return 0;
  //return encoderX264->encodeFrame(data, stride, pOutStream, pFrameType, pOutTimeStamp);
}

VideoEncoderOffline::~VideoEncoderOffline() {
  WriteEndParam(1, 1, rootPath + "InEnd.txt");

  for (int i = 0; i < SLEEP_COUNT; i++) {
    bool hasEnd = false;
    bool bEarlyExit = false;
    bool ret = ReadEndParam(hasEnd, bEarlyExit, rootPath + "OutEnd.txt");
    if (ret && (hasEnd || bEarlyExit)) {
      break;
    } else {
      printf("sleep... VideoEncoderOffline=%d\n", i);
      usleep(SLEEP_US);
    }
  }

  if (data[0] != nullptr) {
    delete data[0];
  }
  if (data[1] != nullptr) {
    delete data[1];
  }
  if (data[2] != nullptr) {
    delete data[2];
  }
  if (h264Buf != nullptr) {
    delete h264Buf;
  }
  for (int i = 0; i < frames; i++) {
    fs::remove((rootPath + "YUV-" + std::to_string(i) + ".yuv").c_str());
    fs::remove((rootPath + "H264-" + std::to_string(i) + ".264").c_str());
    fs::remove((rootPath + "InFrameInfo-" + std::to_string(i) + ".txt").c_str());
    fs::remove((rootPath + "OutFrameInfo-" + std::to_string(i) + ".txt").c_str());
  }
  fs::remove((rootPath + "InEnd.txt").c_str());
  fs::remove((rootPath + "OutEnd.txt").c_str());
  fs::remove((rootPath + "Header-0.dat").c_str());
  fs::remove((rootPath + "Header-1.dat").c_str());
  fs::remove((rootPath + "EncoderParam.txt").c_str());
}
