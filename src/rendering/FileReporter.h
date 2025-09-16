/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <string>
#include <unordered_map>
#include <vector>
#include "pag/pag.h"

namespace pag {
class FileReporter {
 public:
  static std::unique_ptr<FileReporter> Make(std::shared_ptr<PAGLayer> pagLayer);

  explicit FileReporter(File* file);
  ~FileReporter();
  void recordPerformance(RenderCache* cache);

 private:
  void setFileInfo(File* file);
  void reportData();

  std::string pagInfoString;
  int flushCount = 0;

  int64_t presentTotalTime = 0;
  int64_t presentMaxTime = 0;
  int64_t presentFirstFrameTime = 0;

  int64_t renderTotalTime = 0;
  int64_t renderMaxTime = 0;
  int64_t renderFirstFrameTime = 0;

  int64_t flushTotalTime = 0;
  int64_t flushMaxTime = 0;
  int64_t flushFirstFrameTime = 0;

  int64_t imageDecodingMaxTime = 0;

  int64_t hardwareDecodingMaxTime = 0;
  int64_t hardwareDecodingTotalTime = 0;
  int64_t hardwareDecodingInitialTime = 0;
  int hardwareDecodingCount = 0;

  int64_t softwareDecodingTotalTime = 0;
  int64_t softwareDecodingMaxTime = 0;
  int64_t softwareDecodingInitialTime = 0;
  int softwareDecodingCount = 0;

  size_t graphicsMemoryMax = 0;
  size_t graphicsMemoryTotal = 0;
};
}  // namespace pag
