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

#include "OfflineVideoEncoder.h"
#include <pag/file.h>
#include <platform/PlatformHelper.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <thread>
#include "utils/FileHelper.h"

namespace exporter {

constexpr int SleepUS = 1000;
constexpr int SleepCount = 2000;

static std::string GetH264EncoderToolsFolder() {
  return JoinPaths(GetRoamingPath(), "H264EncoderTools");
}

static std::string GetOfflineFolder() {
  return JoinPaths(GetH264EncoderToolsFolder(), "OffLineFolder");
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

static bool WriteYUVData(uint8_t* data[4], int stride[4], int width, int height,
                         const std::string& path) {
  auto fp = fopen(path.c_str(), "wb");
  if (fp == nullptr) {
    return false;
  }
  bool ret = WriteYUVPlane(data[0], stride[0], width / 1, height / 1, fp);
  ret = ret && WriteYUVPlane(data[1], stride[1], width / 2, height / 2, fp);
  ret = ret && WriteYUVPlane(data[2], stride[2], width / 2, height / 2, fp);
  fflush(fp);
  fclose(fp);
  return ret;
}

OfflineVideoEncoder::~OfflineVideoEncoder() {
  close();
  exit();
}

bool OfflineVideoEncoder::open(int width, int height, double frameRate, bool hasAlpha,
                               int maxKeyFrameInterval, int quality) {
  this->width = width;
  this->height = height;
  this->frameRate = frameRate;

  param.width = width;
  param.height = height;
  param.frameRate = frameRate;
  param.hasAlpha = hasAlpha;
  param.quality = quality;
  param.maxKeyFrameInterval = maxKeyFrameInterval;

  stride[0] = SIZE_ALIGN(width);
  stride[1] = SIZE_ALIGN(stride[0] / 2);
  stride[2] = stride[1];
  h264Buf.resize(width * height + 1024);
  data[0].resize(stride[0] * (SIZE_ALIGN(height) / 1), 128);
  data[1].resize(stride[1] * (SIZE_ALIGN(height) / 2), 128);
  data[2].resize(stride[2] * (SIZE_ALIGN(height) / 2), 128);

  rootPath = GetOfflineFolder();
  DeleteFile(rootPath);
  CreateDir(rootPath);
  std::string encodeParamFilePath = JoinPaths(rootPath, "EncoderParam.txt");
  std::string paramStr =
      QString(
          R"({ "Width": %1, "Height": %2, "FrameRate": %3, "HasAlpha": %4, "MaxKeyFrameInterval": %5, "Quality": %6 })")
          .arg(width)
          .arg(height)
          .arg(frameRate, 0, 'f', 2)
          .arg(hasAlpha)
          .arg(maxKeyFrameInterval)
          .arg(quality)
          .toStdString();
  WriteTextFile(encodeParamFilePath, paramStr);

  QString workingDirectory = QString::fromStdString(GetH264EncoderToolsFolder());
  QString toolExecutable =
      QString::fromStdString(JoinPaths(workingDirectory.toStdString(), "H264EncoderTools"));
#ifdef _WIN32
  if (!toolExecutable.endsWith(".exe")) {
    toolExecutable.append(".exe");
  }
#endif
  toolExecutable = QDir::toNativeSeparators(toolExecutable);
  QString offlineFolderPath = QString::fromStdString(GetOfflineFolder());
  offlineFolderPath = QDir::toNativeSeparators(offlineFolderPath);
  if (!offlineFolderPath.endsWith(QDir::separator())) {
    offlineFolderPath.append(QDir::separator());
  }
  QStringList arguments;
  arguments << offlineFolderPath;
  process = std::make_unique<QProcess>();
  process->setWorkingDirectory(workingDirectory);
  process->start(toolExecutable, arguments);

  return process->waitForStarted();
}

void OfflineVideoEncoder::close() {
  if (process == nullptr) {
    return;
  }
  std::string endFile = JoinPaths(rootPath, "InEnd.txt");
  writeEndParam(true, false, endFile);
}

void OfflineVideoEncoder::exit() {
  if (process->state() == QProcess::Running) {
    process->waitForFinished(10000);
  }
  bool hasEnd = false;
  bool earlyExit = false;
  std::string outEndFile = JoinPaths(rootPath, "OutEnd.txt");
  bool ret = readEndParam(hasEnd, earlyExit, outEndFile);
  if (!ret || !(hasEnd || earlyExit)) {
    process->kill();
  }
  process = nullptr;
}

void OfflineVideoEncoder::getInputFrameBuf(uint8_t* data[], int stride[]) {
  data[0] = this->data[0].data();
  data[1] = this->data[1].data();
  data[2] = this->data[2].data();
  stride[0] = this->stride[0];
  stride[1] = this->stride[1];
  stride[2] = this->stride[2];
}

int OfflineVideoEncoder::encodeHeaders(uint8_t* header[], int headerSize[]) {
  for (int i = 0; i < SleepCount; i++) {
    std::string header0 = JoinPaths(rootPath, "Header-0.dat");
    std::string header1 = JoinPaths(rootPath, "Header-1.dat");
    int size0 = ReadFileData(header0, this->header[0].data(), this->header[0].size());
    int size1 = ReadFileData(header1, this->header[1].data(), this->header[1].size());
    if (size0 > 0 && size1 > 0) {
      header[0] = this->header[0].data();
      header[1] = this->header[1].data();
      headerSize[0] = size0;
      headerSize[1] = size1;
      return 2;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(SleepUS));
  }
  return 0;
}

void OfflineVideoEncoder::encodeFrame(uint8_t* data[], int stride[], FrameType frameType) {
  if (data[0] == nullptr) {
    return;
  }

  std::string yuvFile = JoinPaths(rootPath, "YUV-" + std::to_string(frames) + ".yuv");
  WriteYUVData(data, stride, width, height, yuvFile);

  FrameInfo frameInfo;
  frameInfo.frameType = frameType;
  frameInfo.timeStamp = frames;
  frameInfo.frameSize = width * height * 3 / 2;

  std::string frameInfoFile = JoinPaths(rootPath, "InFrameInfo-" + std::to_string(frames) + ".txt");
  writeFrameInfo(frameInfo, frameInfoFile);
  frames++;
}

int OfflineVideoEncoder::getEncodedFrame(bool wait, uint8_t** outData, FrameType* outFrameType,
                                         int64_t* outFrameIndex) {
  for (int i = 0; i < SleepCount && outFrames < frames; i++) {
    FrameInfo outFrameInfo;
    std::string outFrameInfoFile =
        JoinPaths(rootPath, "OutFrameInfo-" + std::to_string(outFrames) + ".txt");
    bool ret = readFrameInfo(outFrameInfo, outFrameInfoFile);
    if (ret) {
      std::string h264File = JoinPaths(rootPath, "H264-" + std::to_string(outFrames) + ".264");
      int size = ReadFileData(h264File, h264Buf.data(), h264Buf.size());
      if (size > 0 && size == outFrameInfo.frameSize) {
        printf("read frame=%d size = %d outSize = %d\n", outFrames, size, outFrameInfo.frameSize);
        *outData = h264Buf.data();
        *outFrameType = outFrameInfo.frameType;
        *outFrameIndex = outFrameInfo.timeStamp;
        outFrames++;
        return size;
      }
    }
    if (process->state() != QProcess::Running) {
      qDebug() << "H264EncoderTools abnormal exit: " << process->exitCode();
      qDebug() << "Error: " << process->error();
      qDebug() << "Standard Error: " << process->readAllStandardError().data();
      return -1;
    }
    if (wait) {
      std::this_thread::sleep_for(std::chrono::microseconds(SleepUS));
    } else {
      break;
    }
  }

  return 0;
}

bool OfflineVideoEncoder::readEndParam(bool& hasEnd, bool& earlyExit, const std::string& filePath) {
  QFile file(filePath.data());
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
  file.close();
  if (parseError.error != QJsonParseError::NoError) {
    return false;
  }

  if (doc.isObject()) {
    QJsonObject obj = doc.object();
    hasEnd = obj.value("HasEnd").toInt() == 1;
    earlyExit = obj.value("EarlyExit").toInt() == 1;
    return true;
  }
  return false;
}

bool OfflineVideoEncoder::writeEndParam(bool hasEnd, bool earlyExit, const std::string& filePath) {
  std::string endData = QString(R"({ "HasEnd": %1, "EarlyExit": %2 })")
                            .arg(hasEnd ? 1 : 0)
                            .arg(earlyExit ? 1 : 0)
                            .toStdString();
  return WriteTextFile(filePath, endData) != 0;
}

bool OfflineVideoEncoder::readFrameInfo(FrameInfo& frameInfo, const std::string& filePath) {
  QFile file(filePath.data());
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
  file.close();
  if (parseError.error != QJsonParseError::NoError) {
    return false;
  }

  if (doc.isObject()) {
    QJsonObject obj = doc.object();
    frameInfo.frameType = static_cast<FrameType>(obj.value("FrameType").toInt());
    frameInfo.timeStamp = obj.value("TimeStamp").toInt();
    frameInfo.frameSize = obj.value("FrameSize").toInt();
    return true;
  }
  return false;
}

bool OfflineVideoEncoder::writeFrameInfo(const FrameInfo& frameInfo, const std::string& filePath) {
  std::string frameInfoData = QString(R"({ "FrameType": %1, "TimeStamp": %2, "FrameSize": %3 })")
                                  .arg(static_cast<int>(frameInfo.frameType))
                                  .arg(static_cast<float>(frameInfo.timeStamp), 0, 'f', 0)
                                  .arg(frameInfo.frameSize)
                                  .toStdString();
  return WriteTextFile(filePath, frameInfoData) != 0;
}

}  // namespace exporter
