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
  // HTML/URL inputs only: when true, the html-snapshot pre-pass is asked to
  // capture the page's animations (CSS @keyframes, Web Animations, GSAP,
  // anime.js) into the subset (`pagx import --html-capture-animations`, forwarded as
  // `--capture-animations` to standalone snapshot.js) so the importer can replay the motion.
  // Off by default: the snapshot emits a single static frame.
  bool captureAnimations = false;
  // Replay an HTML interaction scenario and import one named static checkpoint.
  // Public import flags are --html-scenario and --html-checkpoint; the standalone
  // snapshot helper uses --scenario and --checkpoint.
  std::string scenario = {};
  std::string checkpoint = {};
};

/**
 * Parses the shared SVG importer flags used by resolve. Unrecognized arguments are
 * silently skipped. Public import arguments are parsed by ParseImportRequest.
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
