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
#include <vector>

namespace pagx {

// Export warnings for the PAG v4 pipeline (docs/pagx_to_pag_interfaces.md §1). A warning never
// blocks an export by itself; --strict turns every warning into a failure.
enum class PAGExportWarning {
  // A font referenced by the document is not registered for export-time measuring.
  MissingFont,
  // An external image is missing or failed to decode; the Image keeps no data.
  MissingImage,
  // An external sub-document is missing or failed to parse; the layer exports empty.
  MissingSubDocument,
  // A sub-document or image path is a URL, which the offline pipeline cannot load.
  UnsupportedURL,
  // Phase 1: animated content is skipped and only the static picture is exported.
  AnimatedContentSkipped,
  // An animation or data bind targets a layout-only channel, which can never be animated;
  // the binding is dropped (design doc §4.2).
  LayoutChannelDropped,
  // A TextBox mixes embedded and runtime-shaped texts; the whole group is laid out at runtime.
  MixedEmbeddedText,
  // An embedded-glyph text is driven by a data bind or animation; after modification it
  // depends on host-registered fonts.
  EmbeddedTextBound,
  // duration <= 0 or frameRate <= 0; the animation is kept and warns.
  InvalidAnimationTiming,
  // A non-finite value (Inf) was rejected while encoding.
  NonFiniteValue,
  // An image was scaled down by the maximum display scale.
  ImageScaledDown,
};

// One warning collected during an export step. nodePath locates the affected node for logs;
// it is a best-effort human-readable path, not a stable identifier.
struct PAGExportIssue {
  PAGExportWarning code = PAGExportWarning::MissingFont;
  std::string message = {};
  // The document the issue belongs to: "main" or a sub-document's diagnostic name.
  std::string scope = {};
  std::string nodePath = {};
};

}  // namespace pagx
