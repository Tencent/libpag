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

#include "base/utils/Log.h"
#include "pag/types.h"
#include "tgfx/core/Clock.h"

namespace pag {
class Performance {
 public:
  virtual ~Performance() = default;
  // ======= total time ==========
  int64_t renderingTime = 0;
  int64_t presentingTime = 0;
  int64_t textureUploadingTime = 0;
  int64_t programCompilingTime = 0;
  int64_t imageDecodingTime = 0;
  int64_t hardwareDecodingTime = 0;
  int64_t softwareDecodingTime = 0;
  // ======= total time ==========

  int64_t hardwareDecodingInitialTime = 0;
  int64_t softwareDecodingInitialTime = 0;
  int64_t totalTime = 0;

  /**
   * Returns the formatted  string which contains the performance data.
   */
  std::string getPerformanceString() const;

  void printPerformance(Frame currentFrame) const;

 protected:
  void resetPerformance();
};
}  // namespace pag
