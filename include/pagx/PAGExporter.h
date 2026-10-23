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
//  either express or implied. See the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace pagx {

enum class PAGExportWarning {
  MissingFont,
  MissingImage,
  MissingSubDocument,
  UnsupportedURL,
  AnimatedContentSkipped,
  LayoutChannelDropped,
  MixedEmbeddedText,
  EmbeddedTextBound,
  InvalidAnimationTiming,
  NonFiniteValue,
  ImageScaledDown,
  UnsupportedColorSource,
  SubDocumentExpansionLimit,
};

struct PAGExportIssue {
  PAGExportWarning code = PAGExportWarning::MissingFont;
  std::string message = {};
  std::string scope = {};
  std::string nodePath = {};
};

struct PAGExportOptions {
  bool strict = false;

  /**
   * When non-empty, a deterministic export report (JSON, interface draft §4) is written to this
   * path: sizes, per-document statistics, warnings, and required fonts. Two exports of the same
   * input produce byte-identical reports.
   */
  std::string reportPath;

  /**
   * The input file's path, recorded in the report's "input" field. Empty when the document did
   * not come from a file.
   */
  std::string inputPath;

  /**
   * Fallback fonts for export-time text measurement (design doc §4.3), same semantics as the
   * embed and render commands: each entry is a font file path or a system font name in
   * "family[,style]" form. The document's own Font file declarations register first; these
   * supplement them. Entries that cannot be resolved produce MissingFont warnings.
   */
  std::vector<std::string> fallbackFonts;
};

class PAGXDocument;

struct PAGExportResult {
  bool success = false;
  std::vector<PAGExportIssue> warnings;
  size_t byteSize = 0;
};

class PAGExporter {
 public:
  /**
   * Exports a PAGX document to the PAG v4 binary format. The document is cloned before
   * processing, so the caller's document is not modified. The full pipeline runs:
   * flatten (T2) → strip (T3) → image processing (T4) → binary encoding.
   * Phase 1 skips animated content with a warning (design doc §12.3).
   */
  static PAGExportResult ExportToPAG(std::shared_ptr<PAGXDocument> document,
                                      const std::string& filePath,
                                      const PAGExportOptions& options = {});
};

}  // namespace pagx
