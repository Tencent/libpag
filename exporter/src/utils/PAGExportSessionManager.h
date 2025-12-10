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

#include "PAGExportSession.h"

namespace exporter {

class PAGExportSessionManager {
 public:
  static PAGExportSessionManager* GetInstance() {
    static PAGExportSessionManager instance;
    return &instance;
  }

  PAGExportSessionManager(const PAGExportSessionManager&) = delete;
  PAGExportSessionManager& operator=(const PAGExportSessionManager&) = delete;

  void setCurrentSession(std::shared_ptr<PAGExportSession> session);
  void unsetCurrentSession(std::shared_ptr<PAGExportSession> session);
  void recordWarning(AlertInfoType type, const std::string& addInfo = "");
  float getCurrentCompositionFrameRate();
  pag::GradientColorHandle getGradientColors(const std::vector<std::string>& matchNames, int index,
                                             int keyFrameIndex = 0);

 private:
  PAGExportSessionManager() = default;
  std::shared_ptr<PAGExportSession> currentSession = nullptr;
};

}  // namespace exporter
