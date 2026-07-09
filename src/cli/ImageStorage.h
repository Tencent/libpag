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

namespace pagx::cli {

/**
 * How Image resources are stored in an exported PAGX document.
 */
enum class ImageStorageMode {
  /// Write image bytes to files on disk next to the PAGX output and reference them by their
  /// original relative path (`<Image source="dir/name.png"/>`). This is the default: it keeps the
  /// PAGX small and produces a portable folder that travels with the document.
  External,
  /// Inline image bytes directly into the PAGX as base64 data URIs
  /// (`<Image source="data:image/png;base64,..."/>`), producing a single self-contained file.
  Embed,
};

/**
 * Parses an image-storage mode name (case-insensitive: "external" or "embed"). On success writes
 * the parsed mode to *mode and returns true; on an unknown value leaves *mode untouched and
 * returns false.
 */
bool ParseImageStorageMode(const std::string& name, ImageStorageMode* mode);

/**
 * Configuration for ApplyImageStorage.
 */
struct ImageStorageOptions {
  /// Target storage mode. Defaults to External ("image output").
  ImageStorageMode mode = ImageStorageMode::External;
  /// Directory used to resolve relative Image `source` paths to real files on disk. When empty the
  /// helper falls back to `outputDir`. Relative paths that cannot be resolved against this base are
  /// left untouched (a warning is emitted).
  std::string baseDir = {};
  /// Directory the source document lives in — i.e. the prefix `PAGXImporter::FromFile` (or the HTML
  /// importer) bakes onto every relative Image path so it can be located at load time. It is
  /// stripped back off to recover the authored relative `source`, independently of `baseDir` (which
  /// may point elsewhere, e.g. the original HTML when resolving an intermediate PAGX). When empty
  /// only `baseDir` is used for prefix recovery.
  std::string documentDir = {};
  /// Directory the exported PAGX will be written to. In External mode, copied image files land here
  /// (preserving their relative sub-path); it is also where base64/data-URI images are materialised
  /// when converting to External. Required for External mode.
  std::string outputDir = {};
};

/**
 * Normalises every Image resource in `doc` to the requested storage mode, generically and in place:
 *
 *   - External: local file-path images are copied under `outputDir` at the same relative path (the
 *     `source` string is preserved unchanged — "path names do not change"). Images already carried
 *     as embedded bytes or data URIs are left exactly as they are; inline content is never extracted
 *     out to a file.
 *   - Embed: image bytes are read from the resolved local file and inlined so the exporter emits a
 *     base64 data URI. Images already inline (embedded bytes / data URIs) are left untouched.
 *
 * Absolute local paths and remote URLs (http/https) are left untouched. Missing sources are left as
 * references with a warning. Diagnostics are printed to stderr prefixed with `command`.
 */
void ApplyImageStorage(PAGXDocument* doc, const ImageStorageOptions& options,
                       const std::string& command);

}  // namespace pagx::cli
