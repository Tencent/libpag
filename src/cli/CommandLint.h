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
 * Runs visual quality checks (VIS-xxx rules) on a PAGX document loaded from the given file.
 * Returns all issues found. An empty vector means no issues detected.
 */
std::vector<LintIssue> LintFile(const std::string& filePath);

/**
 * Checks visual quality rules against a pre-loaded document.
 * Covers VIS-001/002/003 (pixel alignment), VIS-010/011/012/013 (stroke width),
 * VIS-020/021/022 (safe zone), and VIS-100/101 (color hardcoding).
 */
std::vector<LintIssue> LintDocument(const pagx::PAGXDocument* document);

/**
 * Reports visual quality issues for a PAGX file. Always returns 0 — lint results are advisory
 * only and must not block the generation pipeline.
 * Supports --format json for structured output.
 */
int RunLint(int argc, char* argv[]);

}  // namespace pagx::cli
