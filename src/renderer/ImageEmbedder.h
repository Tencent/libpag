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

#include "pagx/PAGXDocument.h"

namespace pagx {

/**
 * ImageEmbedder reads every external-file Image node in a PAGXDocument and inlines the raw
 * bytes via PAGXDocument::loadFileData. Unlike FontEmbedder, it does not require applyLayout()
 * and has no ClearEmbeddedGlyphRuns-style helper — Image embedding has no layout dependency.
 *
 * URL-form paths (containing "://") are silently skipped; production PAGX does not use them.
 *
 * On the first read failure, embed() returns false and lastErrorPath() identifies the
 * offending file. The document may be partially mutated on failure (earlier successful
 * reads are already applied); callers must not write the output on failure.
 */
class ImageEmbedder {
 public:
  ImageEmbedder() = default;

  /**
   * Embeds every external Image file referenced by the document. Returns true on full
   * success. On failure, returns false and sets lastErrorPath() to the first unreadable
   * file.
   */
  bool embed(PAGXDocument* document);

  /**
   * When embed() returns false, identifies which file could not be read.
   */
  const std::string& lastErrorPath() const {
    return lastErrorPath_;
  }

 private:
  std::string lastErrorPath_;
};

}  // namespace pagx
