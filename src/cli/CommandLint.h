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

namespace pagx {
class PAGXDocument;
}

namespace pagx::cli {

struct LintIssue {
  std::string ruleId = {};
  std::string location = {};
  std::string message = {};
};

/**
 * Runs visual quality checks on a PAGX document loaded from the given file.
 * Returns all issues found. An empty vector means no issues detected.
 */
std::vector<LintIssue> LintFile(const std::string& filePath);

/**
 * Checks visual quality rules against a pre-loaded document.
 * Covers pixel alignment, stroke width constraints, safe zone margins, and theme color usage.
 */
std::vector<LintIssue> LintDocument(const pagx::PAGXDocument* document);

/**
 * Reports visual quality issues for a PAGX file. Always returns 0 — lint results are advisory
 * only and must not block the generation pipeline.
 * Supports --json option for structured output.
 */
int RunLint(int argc, char* argv[]);

}  // namespace pagx::cli
