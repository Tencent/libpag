/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "pagx/PAGXDocument.h"
#include "tgfx/core/Typeface.h"

namespace tgfx {
class TextShaper;
}

namespace pagx {

/**
 * TextPrecomposer processes all Text elements in a PAGXDocument, performing text layout/shaping
 * and converting them to precomposed GlyphRun format with embedded Font resources. This enables
 * cross-platform rendering consistency without runtime font dependencies.
 */
class TextPrecomposer {
 public:
  struct Options {
    /**
     * Fallback typefaces for text shaping.
     */
    std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces;

    /**
     * Custom text shaper for advanced text layout.
     * If not provided, a default TextShaper will be created from fallbackTypefaces.
     */
    std::shared_ptr<tgfx::TextShaper> textShaper;

    Options() = default;
  };

  /**
   * Processes all Text elements in the document, converting runtime-shaped text to precomposed
   * GlyphRun format. For each unique Typeface used, a Font resource is added to the document with
   * embedded glyph outlines. Original text content and font properties are preserved for editing.
   *
   * @param document The document to process (modified in place).
   * @param options Configuration options for text shaping.
   * @return true if any text was processed, false if no text elements found or all already
   *         precomposed.
   */
  static bool Process(PAGXDocument& document, const Options& options);
};

}  // namespace pagx
