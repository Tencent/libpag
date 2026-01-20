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
 * PAGXSVGParser converts SVG documents to PAGXDocument.
 * This parser is independent of tgfx and preserves complete SVG information.
 */
class PAGXSVGParser {
 public:
  struct Options {
    /**
     * If true, unsupported SVG elements are preserved as Unknown nodes.
     */
    bool preserveUnknownElements;

    /**
     * If true, <use> references are expanded to actual content.
     */
    bool expandUseReferences;

    /**
     * If true, nested transforms are flattened into single matrices.
     */
    bool flattenTransforms;

    Options() : preserveUnknownElements(false), expandUseReferences(true), flattenTransforms(false) {
    }
  };

  /**
   * Parses an SVG file and creates a PAGXDocument.
   */
  static std::shared_ptr<PAGXDocument> Parse(const std::string& filePath,
                                             const Options& options = Options());

  /**
   * Parses SVG data and creates a PAGXDocument.
   */
  static std::shared_ptr<PAGXDocument> Parse(const uint8_t* data, size_t length,
                                             const Options& options = Options());

  /**
   * Parses an SVG string and creates a PAGXDocument.
   */
  static std::shared_ptr<PAGXDocument> ParseString(const std::string& svgContent,
                                                   const Options& options = Options());
};

}  // namespace pagx
