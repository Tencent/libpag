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

pag::GradientColorHandle PAGExportSessionManager::getGradientColors(
    const std::vector<std::string>& matchNames, int index) {
  if (currentSession) {
    return currentSession->GetGradientColorsFromFileBytes(matchNames, index);
  }
  return GetDefaultGradientColors();
}

}  // namespace exporter