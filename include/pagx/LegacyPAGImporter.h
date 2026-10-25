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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "pagx/PAGExporter.h"

namespace pag {
class File;
}

namespace pagx {

/**
 * Converts a decoded legacy PAG file (pag::File) into a PAGXDocument in memory, without
 * generating or re-parsing any XML text (design doc §7.1). The converted document runs
 * through the same publish pipeline as a PAGX source, so every phase-1/2 quality guarantee
 * applies.
 *
 * Phase-3 skeleton scope (P3-4): composition/layer trees, Transform2D static values mapped
 * to the decomposed Layer fields, solid/shape layers with solid fills and strokes, nested
 * precompositions with the time-lock mount, and the always-present root animation fallback.
 * Keyframes, text, images, effects, styles, and masks land with P3-3/P3-5..P3-9; anything
 * the skeleton cannot represent yet is reported through the issues list.
 */
class LegacyPAGImporter {
 public:
  /**
   * Converts the given legacy file into a new PAGXDocument. Returns nullptr when the file
   * cannot be converted at all. The input file is only read, never modified. Each
   * unrepresentable feature appends a PAGExportIssue to issues (when non-null) so callers
   * can run --strict semantics on top.
   */
  static std::shared_ptr<PAGXDocument> Convert(const std::shared_ptr<pag::File>& file,
                                               std::vector<PAGExportIssue>* issues = nullptr);
};

}  // namespace pagx
