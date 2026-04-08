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
#include "pagx/nodes/Element.h"

namespace pagx {

/**
 * Import is a build directive that embeds external content (e.g., SVG) into a PAGX file. It appears
 * inside a Layer at the VectorElement level and must be resolved into native PAGX nodes by
 * `pagx import --resolve` before layout or rendering.
 *
 * Two modes:
 * - External: `source` contains a file path relative to the PAGX file.
 * - Inline: `source` is empty; `rawContent` contains the original XML text between the
 *   `<Import>` and `</Import>` tags.
 */
class Import : public Element {
 public:
  /**
   * Path to external file, relative to the PAGX file. Empty for inline mode.
   */
  std::string source = {};

  /**
   * Forced input format (e.g., "svg"). When empty, inferred from file extension or inline content.
   */
  std::string format = {};

  /**
   * Original XML text of inline content between `<Import>` and `</Import>` tags. Empty for external
   * mode.
   */
  std::string rawContent = {};

  NodeType nodeType() const override {
    return NodeType::Import;
  }

 private:
  Import() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
