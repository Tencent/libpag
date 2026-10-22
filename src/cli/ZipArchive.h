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

namespace pagx::cli {

/**
 * A zip archive extracted into a private temporary directory. The directory and everything in it
 * are removed when this object is destroyed, so callers keep it alive for as long as they read the
 * extracted files.
 */
class ExtractedArchive {
 public:
  ExtractedArchive() = default;
  ~ExtractedArchive();

  ExtractedArchive(const ExtractedArchive&) = delete;
  ExtractedArchive& operator=(const ExtractedArchive&) = delete;

  /**
   * Extracts the zip archive at zipPath into a new temporary directory. Entries whose path is
   * absolute or climbs out of the directory through `..` are rejected, so an archive can never
   * write outside the directory it is extracted into. Returns false and sets error on failure.
   */
  bool extract(const std::string& zipPath, std::string* error);

  /**
   * Returns the directory the archive was extracted into, or an empty string before a successful
   * extract().
   */
  const std::string& directory() const {
    return _directory;
  }

  /**
   * Returns the path of the page an extracted HTML export opens with: the shallowest index.html,
   * or the only HTML file when there is no index.html. Returns an empty string and sets error when
   * the archive holds no HTML file, or several of them but no index.html.
   */
  std::string findEntryHTML(std::string* error) const;

 private:
  std::string _directory = {};
};

}  // namespace pagx::cli
