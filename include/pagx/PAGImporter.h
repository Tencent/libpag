/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "pagx/PAGXDocument.h"

namespace pagx {

enum class PAGFormat { PAGLegacy, PAGv4, PAGX, Unknown };

struct PAGImportResult {
  bool success = false;
  PAGFormat format = PAGFormat::Unknown;
  std::string error;
  uint16_t tagLevel = 0;
};

class PAGImporter {
 public:
  static PAGFormat DetectFormat(const std::string& filePath);
  static PAGFormat DetectFormat(const void* bytes, size_t length);

  /**
   * Loads a PAG v4 binary file into a PAGXDocument. Returns nullptr on failure.
   * The decoded document has layout already flattened (§4.2); PAGScene::Make will
   * re-measure it on load.
   */
  static std::shared_ptr<PAGXDocument> LoadFromPAG(const std::string& filePath,
                                                    PAGImportResult* result = nullptr);
};

}  // namespace pagx
