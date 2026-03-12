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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>

namespace pagx::cli {

struct ValidationError {
  int line = 0;
  std::string message = {};
};

/**
 * Validates a PAGX file against the XSD specification schema.
 * Returns an empty vector if the file is valid.
 */
std::vector<ValidationError> ValidateFile(const std::string& filePath);

/**
 * Validates a PAGX file against the specification. Standalone command — note that
 * `pagx optimize` already includes validation as its first step.
 * Supports --format json for structured output.
 */
int RunValidate(int argc, char* argv[]);

}  // namespace pagx::cli
