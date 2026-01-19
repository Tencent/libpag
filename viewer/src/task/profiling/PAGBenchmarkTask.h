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

#pragma once

#include "codec/DataTypes.h"
#include "task/PAGTask.h"

namespace pag {

class PAGBenchmarkTask : public PAGPlayTask {
 public:
  PAGBenchmarkTask(std::shared_ptr<PAGFile> pagFile, const QString& filePath);

 protected:
  void onBegin() override;
  int onFinish() override;
  void onFrameFlush(double progress) override;

 public:
  std::shared_ptr<PerformanceData> avgPerformanceData = nullptr;
  std::shared_ptr<PerformanceData> maxPerformanceData = nullptr;
  std::shared_ptr<PerformanceData> firstFramePerformanceData = nullptr;
};

}  // namespace pag
