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
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

namespace pagx {

class PAGXDocument;

/**
 * Loads every external file a PAGX document references from local disk (design doc §4.8):
 * external composition files (layer composition paths) and Image files. Composition paths
 * resolve against the directory of the file that wrote the reference — baseDir for the root
 * document, each sub-file's own directory for nested references. The same absolute file is
 * read only once; reference chains are cycle-checked by the existing loadFileData chain
 * guard; the nesting depth and total sub-document instance count are capped per spec §11.
 *
 * Intended for CLI tools and tests; hosts with their own file access keep driving
 * getExternalFilePaths()/loadFileData() directly.
 */
class PAGXExternalFiles {
 public:
  /**
   * Recursively loads the document's external files into it via loadFileData so the existing
   * wrapper composition, layout, and cycle-detection machinery runs unchanged.
   * @param document the document to load files into. Must be non-null.
   * @param baseDir the directory of the PAGX file the document came from, with or without a
   * trailing slash. Relative references resolve against it.
   * @return True when every referenced file was found and loaded; false when some file was
   * missing or unreadable (the document stays usable and unresolved layers/images render
   * empty, matching the XML runtime behavior).
   */
  static bool Load(PAGXDocument* document, const std::string& baseDir);
};

}  // namespace pagx
