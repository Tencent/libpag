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

#include <memory>
#include <string>
#include <vector>

namespace pag {

class PAGReporter {
 public:
  static PAGReporter* GetInstance() {
    if (instance == nullptr) {
      instance = new PAGReporter();
    }
    return instance;
  }
  void report();
  void setEvent(const std::string& event);
  void addParam(const std::string& key, const std::string& value);
  void setAppKey(const std::string& appKey);
  void setAppName(const std::string& appName);
  void setAppVersion(const std::string& appVersion);
  void setAppBundleId(const std::string& appBundleId);
  void setPAGCount(const std::string& pagCount);
  void setPAGExportEntry(const std::string& pagExportEntry);

  PAGReporter(const PAGReporter& s) = delete;
  PAGReporter(const PAGReporter&& s) = delete;
  PAGReporter& operator=(const PAGReporter& s) = delete;

 private:
  PAGReporter();

  static PAGReporter* instance;
};

}  // namespace pag
