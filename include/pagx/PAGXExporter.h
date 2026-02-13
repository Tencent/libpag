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
#include "pagx/PAGXDocument.h"

namespace pagx {

/**
 * Export options for PAGXExporter.
 */
struct PAGXExportOptions {
  /**
   * Whether to skip GlyphRun and Font resource data in the export.
   * - false (default): Export pre-shaped glyph data for cross-platform consistency.
   * - true: Skip glyph data to reduce file size. Text will require runtime shaping.
   */
  bool skipGlyphData = false;
};

/**
 * PAGXExporter exports PAGXDocument to PAGX XML format.
 */
class PAGXExporter {
 public:
  using Options = PAGXExportOptions;

  /**
   * Exports a PAGXDocument to XML string.
   * The output faithfully reflects the structure of the input document.
   */
  static std::string ToXML(const PAGXDocument& document, const Options& options = {});
};

}  // namespace pagx
