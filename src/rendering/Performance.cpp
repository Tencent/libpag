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

#include "Performance.h"
#include <cstdio>

namespace pag {
std::string Performance::getPerformanceString() const {
  char buffer[300];
  snprintf(buffer, 300,
           "%6.1fms[Render] %6.1fms[Image] %6.1fms[Video]"
           " %6.1fms[Texture] %6.1fms[Program] %6.1fms[Present] ",
           static_cast<double>(renderingTime) / 1000.0,
           static_cast<double>(imageDecodingTime) / 1000.0,
           static_cast<double>(softwareDecodingTime + hardwareDecodingTime) / 1000.0,
           static_cast<double>(textureUploadingTime) / 1000.0,
           static_cast<double>(programCompilingTime) / 1000.0,
           static_cast<double>(presentingTime) / 1000.0);
  return buffer;
}

void Performance::printPerformance(Frame currentFrame) const {
  auto performance = getPerformanceString();
  LOGI("%4d | %6.1fms :%s", currentFrame, static_cast<double>(totalTime) / 1000.0,
       performance.c_str());
}

void Performance::resetPerformance() {
  renderingTime = 0;
  presentingTime = 0;
  imageDecodingTime = 0;
  hardwareDecodingTime = 0;
  softwareDecodingTime = 0;
  textureUploadingTime = 0;
  programCompilingTime = 0;

  hardwareDecodingInitialTime = 0;
  softwareDecodingInitialTime = 0;
  totalTime = 0;
}
}  // namespace pag
