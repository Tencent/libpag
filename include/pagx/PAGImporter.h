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
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <string>
#include "pagx/PAGXDocument.h"

namespace pagx {

// The four shapes of input the SDK can be handed (spec/pag_binary_spec.md §3.6).
enum class PAGFormat {
  // Legacy binary PAG, file header version 0-3.
  PAGLegacy,
  // The PAG v4 binary format produced by PAGExporter.
  PAGv4,
  // XML PAGX.
  PAGX,
  // Anything else.
  Unknown,
};

/**
 * PAGImporter loads binary PAG files into PAGX documents. Phase 1 of the v4 project adds
 * format detection and the version-4 file header; full v4 decoding and the legacy
 * converter arrive with the importer tasks of the implementation plan.
 */
class PAGImporter {
 public:
  /**
   * Detects the format of a file by reading its first 16 bytes (spec §3.6): PAG headers
   * route on the version byte, a UTF-8 BOM or leading whitespace followed by '<' means
   * PAGX. Returns Unknown for empty, truncated, or unrecognized content.
   */
  static PAGFormat DetectFormat(const std::string& filePath);

  /**
   * Same detection over in-memory bytes.
   */
  static PAGFormat DetectFormat(const void* bytes, size_t length);
};

}  // namespace pagx
