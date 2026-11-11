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

#include <memory>
#include <string>
#include "pagx/PAGXDocument.h"
#include "pagx/types/Data.h"

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

  /**
   * Exports a PAGXDocument to an in-memory ZIP archive. The archive always contains
   * `document.pagx`, and every image whose payload is available is written as
   * `images/<name>.<ext>` with the XML referring to that entry instead of a data URI. A payload
   * is available when Image::data is set, or when Image::filePath holds a base64 `data:` URI,
   * which is decoded before packing.
   *
   * Nothing is read from disk or downloaded, so a local file path, a `hash:` reference or a
   * remote URL keeps its original form and the archive is self-contained only when the caller has
   * supplied every referenced image. Pass external images through
   * PAGXDocument::loadFileData() or PAGXDocument::loadFileDataMap() first. The document itself
   * is not modified.
   *
   * When an archive is produced but is not self-contained, *errorMsg describes which references
   * were left external, along with any packed format `<Image>` cannot render; the ZIP is still
   * returned.
   *
   * @param document the document to export
   * @param options export options
   * @param errorMsg optional output for the failure reason when nullptr is returned, or for the
   *                 self-containment warning when a non-null archive is returned
   * @return the ZIP bytes, or nullptr on failure
   */
  static std::shared_ptr<Data> ToData(const PAGXDocument& document, const Options& options = {},
                                      std::string* errorMsg = nullptr);
};

}  // namespace pagx
