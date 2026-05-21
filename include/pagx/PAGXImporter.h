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

namespace pagx {

/**
 * PAGXImporter parses PAGX XML format into PAGXDocument.
 */
class PAGXImporter {
 public:
  /**
   * Parses a PAGX file and returns a PAGXDocument. Cross-document references in the file (Layer
   * `composition="path.pagx"`) are resolved relative to the file's directory.
   * Returns nullptr if the file cannot be loaded or parsing fails.
   */
  static std::shared_ptr<PAGXDocument> FromFile(const std::string& filePath);

  /**
   * Parses PAGX XML content and returns a PAGXDocument.
   * @param xmlContent the XML source to parse.
   * @param baseDir optional base directory used to resolve relative file paths in cross-document
   *                composition references. When empty, encountering a cross-document reference is
   *                reported as an error.
   * Returns nullptr if parsing fails.
   */
  static std::shared_ptr<PAGXDocument> FromXML(const std::string& xmlContent,
                                               const std::string& baseDir = {});

  /**
   * Parses PAGX XML data and returns a PAGXDocument.
   * @param data raw XML bytes.
   * @param length byte count.
   * @param baseDir optional base directory used to resolve relative file paths in cross-document
   *                composition references.
   * Returns nullptr if parsing fails.
   */
  static std::shared_ptr<PAGXDocument> FromXML(const uint8_t* data, size_t length,
                                               const std::string& baseDir = {});
};

}  // namespace pagx
