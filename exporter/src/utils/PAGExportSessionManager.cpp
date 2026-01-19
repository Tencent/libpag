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

#include "PAGExportSessionManager.h"
#include "AEDataTypeConverter.h"
#include "AEHelper.h"

namespace exporter {

void PAGExportSessionManager::setCurrentSession(std::shared_ptr<PAGExportSession> session) {
  currentSession = session;
}

void PAGExportSessionManager::unsetCurrentSession(std::shared_ptr<PAGExportSession> session) {
  if (currentSession == session) {
    currentSession = nullptr;
  }
}

void PAGExportSessionManager::recordWarning(AlertInfoType type, const std::string& addInfo) {
  if (currentSession) {
    currentSession->pushWarning(type, addInfo);
  }
}

float PAGExportSessionManager::getCurrentCompositionFrameRate() {
  if (currentSession) {
    auto iter = currentSession->itemHandleMap.find(currentSession->compID);
    if (iter != currentSession->itemHandleMap.end()) {
      return GetItemFrameRate(iter->second);
    }
  }
  return 24.0f;
}

pag::GradientColorHandle PAGExportSessionManager::getGradientColors(
    const std::vector<std::string>& matchNames, int index, int keyFrameIndex) {
  if (currentSession) {
    return currentSession->GetGradientColorsFromFileBytes(matchNames, index, keyFrameIndex);
  }
  return GetDefaultGradientColors();
}

}  // namespace exporter
