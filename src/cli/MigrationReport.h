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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pag {
class File;
}

namespace pagx {
class PAGXDocument;
struct PAGExportIssue;
}  // namespace pagx

namespace pagx::cli {

/**
 * Inputs to the legacy-migration fidelity report (spec 3.13): the loaded legacy file (feature
 * inventory), the migrated document (ViewModel property comparison) and the importer's issue
 * records (unsupported-feature details).
 */
struct MigrationReportInput {
  pag::File* legacyFile = nullptr;
  pagx::PAGXDocument* document = nullptr;
  std::vector<pagx::PAGExportIssue>* issues = nullptr;
  std::string inputPath = {};
  uint64_t inputBytes = 0;
  std::string outputPath = {};
  uint64_t outputBytes = 0;
};

/**
 * Renders the fidelity report as Markdown: migration level (fully-migrated / partial / failed),
 * the legacy feature inventory (layers / texts / images / videos / duration / frame rate / size),
 * the per-issue unsupported list, the input-output size comparison and the ViewModel property
 * mapping (editable text<i> / image<i>).
 */
std::string BuildMigrationReport(const MigrationReportInput& input);

/**
 * Prints a one-line summary of the report to stderr (the full report goes to a file or stdout).
 */
void PrintMigrationSummary(const MigrationReportInput& input);

}  // namespace pagx::cli
