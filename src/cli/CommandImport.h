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
};

/**
 * Parses format-specific options (e.g. --svg-*) from argv. Unrecognized arguments are
 * silently skipped.
 */
void ParseFormatOptions(int argc, char* argv[], ImportFormatOptions* options);

/**
 * Imports a file and converts it to a PAGXDocument. Format is inferred from file extension
 * when `format` is empty.
 */
ImportResult ImportFile(const std::string& filePath, const std::string& format,
                        const ImportFormatOptions& formatOptions);

/**
 * Imports from string content and converts it to a PAGXDocument. Format is inferred from
 * the content's root element tag when `format` is empty.
 */
ImportResult ImportString(const std::string& content, const std::string& format,
                          const ImportFormatOptions& formatOptions);

/**
 * Imports a file from another format (e.g. SVG) and converts it to PAGX.
 */
int RunImport(int argc, char* argv[]);

}  // namespace pagx::cli
