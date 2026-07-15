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
#include <vector>
#include "pagx/TextGlyphParams.h"
#include "pagx/TextLayout.h"
#include "pagx/TextLayoutParams.h"
#include "pagx/nodes/Channel.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/layers/vectors/Text.h"

namespace pagx {

class FontConfig;
class LayoutContext;
struct RuntimeTarget;
class Text;

// One Text node driven by a TextHolder: its runtime target (whose bound tgfx::Text is replaced on
// reshape), the current glyph attributes (document values with ViewModel overrides applied in
// place), and the coordinate data needed to rebuild its TextBlob from the shared layout result.
struct TextHolderEntry {
  Text* node = nullptr;
  RuntimeTarget* target = nullptr;
  TextGlyphParams glyph = {};
  tgfx::Matrix inverseMatrix = {};
  float paddingLeft = 0.0f;
  float paddingTop = 0.0f;
};

// Owns the runtime text objects for one text layout source (a standalone Text, or a TextBox with
// its child Texts) and rebuilds them when ViewModel drives text-shaping channels. The pagx
// document is never mutated: overrides are applied to per-entry TextGlyphParams held here, which
// act as the runtime current state (the role a tgfx object plays for other channels). Shaping is
// deferred: apply() only records the change and marks dirty; flush() reshapes once per draw.
class TextHolder {
 public:
  // params: container-level layout parameters snapshotted from the source document at build time
  //         (box size / alignment / writing mode / baseline / textScale). Not overridden.
  explicit TextHolder(TextLayoutParams params) : params(std::move(params)) {
  }

  // Registers a Text node participating in this layout. glyph is the initial current state derived
  // from the node's document fields; inverseMatrix / padding position its rebuilt TextBlob in the
  // node's local coordinate system (identity / zero for a standalone Text).
  void addEntry(Text* node, RuntimeTarget* target, const TextGlyphParams& glyph,
                const tgfx::Matrix& inverseMatrix, float paddingLeft, float paddingTop);

  // ViewModel/Animation -> target. Updates the entry's glyph attribute for the channel (continuous
  // fontSize/letterSpacing mix from the current value; discrete text/font/faux overwrite) and marks
  // the holder dirty. mix follows the existing writer convention: discrete channels ignore it.
  void apply(Text* node, const std::string& channel, const KeyValue& value, float mix);

  // target -> ViewModel. Reads the entry's current effective attribute for the channel.
  bool read(Text* node, const std::string& channel, KeyValue* out) const;

  // Reshapes the whole source and rebuilds every entry's tgfx::Text if dirty. fontConfig is
  // borrowed for the duration of the call to build a LayoutContext; the holder does not retain it.
  void flush(FontConfig* fontConfig);

 private:
  TextHolderEntry* findEntry(Text* node);
  const TextHolderEntry* findEntry(Text* node) const;

  TextLayoutParams params = {};
  std::vector<TextHolderEntry> entries = {};
  bool dirty = false;
};

}  // namespace pagx
