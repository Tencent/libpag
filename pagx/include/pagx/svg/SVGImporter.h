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
#include "tgfx/core/Stream.h"
#include "tgfx/svg/SVGDOM.h"

namespace pagx {

/**
 * SVGImporter provides functionality to import SVG files and convert them to PAGX format.
 */
class SVGImporter {
 public:
  /**
   * Imports an SVG file and converts it to PAGX format.
   * @param svgFilePath The path to the SVG file to import.
   * @return The PAGX content as a string, or an empty string if import fails.
   */
  static std::string ImportFromFile(const std::string& svgFilePath);

  /**
   * Imports SVG content from a stream and converts it to PAGX format.
   * @param svgStream The stream containing SVG content.
   * @return The PAGX content as a string, or an empty string if import fails.
   */
  static std::string ImportFromStream(tgfx::Stream& svgStream);

  /**
   * Imports an SVGDOM object and converts it to PAGX format.
   * @param svgDOM The SVGDOM object to import.
   * @return The PAGX content as a string, or an empty string if import fails.
   */
  static std::string ImportFromDOM(const std::shared_ptr<tgfx::SVGDOM>& svgDOM);

  /**
   * Saves PAGX content to a file.
   * @param pagxContent The PAGX content to save.
   * @param outputPath The path to save the PAGX file.
   * @return True if the file was saved successfully, false otherwise.
   */
  static bool SaveToFile(const std::string& pagxContent, const std::string& outputPath);
};

}  // namespace pagx
