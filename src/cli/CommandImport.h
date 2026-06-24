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

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace pagx {
class PAGXDocument;
}

namespace pagx::cli {

struct ImportResult {
  std::shared_ptr<PAGXDocument> document = nullptr;
  std::string error = {};
  std::vector<std::string> warnings = {};
};

struct ImportFormatOptions {
  bool svgExpandUse = true;
  bool svgFlattenTransforms = false;
  bool svgPreserveUnknown = false;
  bool htmlStrict = false;
  bool htmlPreserveUnknown = false;
  bool htmlPreferBodySize = true;
  /**
   * Run the HTML subset normalizer as a pre-pass inside the importer. Default true. Set to
   * false (via `--html-no-normalize`) when debugging the raw importer behaviour against
   * already-subset HTML.
   */
  bool htmlAutoNormalize = true;
  /**
   * Recover `display: flex` semantics from a flat absolute-positioned input (typically
   * `tools/html-snapshot/snapshot.js` output). Lossy heuristic; default true. Disable via
   * `--html-no-infer-flex`.
   */
  bool htmlInferFlex = true;

  /**
   * Pre-process the HTML input through `tools/html-snapshot/snapshot.js` (a headless-browser
   * snapshotter) before handing it off to the HTML importer. Use this for JS/React-driven
   * pages that only materialise their DOM at runtime: the snapshot script renders the page
   * in Chromium and emits a flat, absolute-positioned HTML subset that the importer can
   * consume directly. Default true. Disable via `--html-no-snapshot`.
   *
   * Requires Node.js on PATH and a working snapshot.js install (run `npm install` inside
   * `tools/html-snapshot`). The snapshot script is located via, in order:
   *   1. `htmlSnapshotBin` (CLI: `--html-snapshot-bin <path>`),
   *   2. the `PAGX_HTML_SNAPSHOT_BIN` environment variable,
   *   3. upward search from cwd for `tools/html-snapshot/snapshot.js`.
   *
   * The snapshot output is the canonical input for `inferFlexFromAbsolute`; the two options
   * are typically used together.
   */
  bool htmlSnapshot = true;

  /**
   * Path to the html-snapshot driver script (`tools/html-snapshot/snapshot.js`). Defaults to
   * the relative path `tools/html-snapshot/snapshot.js`; when it does not resolve as-is the
   * resolver falls back to `PAGX_HTML_SNAPSHOT_BIN` and an upward search from cwd (see
   * `htmlSnapshot`). CLI: `--html-snapshot-bin <path>`.
   */
  std::string htmlSnapshotBin = "tools/html-snapshot/snapshot.js";
};

/**
 * Parses format-specific options (e.g. --svg-*) from argv. Unrecognized arguments are
 * silently skipped.
 */
void ParseFormatOptions(int argc, char* argv[], ImportFormatOptions* options);

/**
 * Imports a file and converts it to a PAGXDocument. Format is inferred from file extension
 * when `format` is empty. When both targetWidth and targetHeight are set (non-NaN), content
 * is uniformly scaled to fit within the target dimensions.
 */
ImportResult ImportFile(const std::string& filePath, const std::string& format,
                        const ImportFormatOptions& formatOptions, float targetWidth = NAN,
                        float targetHeight = NAN);

/**
 * Imports from string content and converts it to a PAGXDocument. Format is inferred from
 * the content's root element tag when `format` is empty. When both targetWidth and
 * targetHeight are set (non-NaN), content is uniformly scaled to fit within the target
 * dimensions.
 */
ImportResult ImportString(const std::string& content, const std::string& format,
                          const ImportFormatOptions& formatOptions, float targetWidth = NAN,
                          float targetHeight = NAN);

/**
 * Imports a file from another format (e.g. SVG) and converts it to PAGX.
 */
int RunImport(int argc, char* argv[]);

}  // namespace pagx::cli
