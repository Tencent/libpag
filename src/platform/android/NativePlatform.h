/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "JNIHelper.h"
#include "platform/Platform.h"

namespace pag {
class NativePlatform : public Platform {
 public:
  static void InitJNI(JNIEnv* env);

  bool hasHardwareDecoder() const override {
    return true;
  }

  std::shared_ptr<PixelBuffer> makeHardwareBuffer(int width, int height,
                                                  bool alphaOnly) const override;

  std::unique_ptr<VideoDecoder> makeHardwareDecoder(const VideoConfig& config) const override;

  std::shared_ptr<Image> makeImage(const std::string& filePath) const override;

  std::shared_ptr<Image> makeImage(std::shared_ptr<Data> imageBytes) const override;

  PAGFont parseFont(const std::string& fontPath, int ttcIndex) const override;

  PAGFont parseFont(const void* data, size_t length, int ttcIndex) const override;

  void printLog(const char format[], ...) const override;

  void printError(const char format[], ...) const override;

  bool registerFallbackFonts() const override;

  void reportStatisticalData(
      std::unordered_map<std::string, std::string>& reportMap) const override;

  void traceImage(const PixelMap& pixelMap, const std::string& tag) const override;
};
}  // namespace pag
