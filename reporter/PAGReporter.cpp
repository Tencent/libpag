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

#include "PAGReporter.h"

namespace pag {

PAGReporter* PAGReporter::instance = nullptr;

PAGReporter::PAGReporter() {
}

void PAGReporter::report() {
}

void PAGReporter::setEvent(const std::string&) {
}

void PAGReporter::addParam(const std::string&, const std::string&) {
}

void PAGReporter::setAppKey(const std::string&) {
}

void PAGReporter::setAppName(const std::string&) {
}

void PAGReporter::setAppVersion(const std::string&) {
}

void PAGReporter::setAppBundleId(const std::string&) {
}

void PAGReporter::setPAGCount(const std::string&) {
}

void PAGReporter::setPAGExportEntry(const std::string&) {
}

}  // namespace pag
