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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace pagx {

/**
 * Unicode helpers the shaper uses to itemise emoji. Both predicates answer a question about one
 * codepoint of a UTF-8 string; the caller decodes the string itself, so neither function has to
 * care about byte offsets.
 */
class EmojiPresentation {
 public:
  /**
   * True when `codepoint` is rendered with an emoji face, either because Unicode gives it emoji
   * presentation by default or because `nextCodepoint` is U+FE0F and explicitly asks for it.
   *
   * The shaper must consult this before the ordinary "first font that contains the codepoint"
   * search: the text faces a Latin font cascades into carry monochrome glyphs for common symbols
   * (U+2764, U+2600, U+2B50, ...), so that search finds a black-and-white glyph long before it
   * reaches the emoji face. A trailing U+FE0E (text presentation) is not a request for emoji, so
   * the caller sees the ordinary, monochrome result there.
   */
  static bool IsEmojiBase(int32_t codepoint, int32_t nextCodepoint);

  /**
   * True for the codepoints that extend the emoji before them instead of standing on their own:
   * the zero-width joiner, the variation selectors, the skin-tone modifiers, the keycap mark and
   * the tag characters used by subdivision flags. They carry no glyph of their own, so they have
   * to be shaped by the font their base used — otherwise the shaper hands the pieces to different
   * fonts and the sequence renders as its separate components (a family of four people instead of
   * one glyph).
   */
  static bool IsEmojiSequenceMark(int32_t codepoint);
};

}  // namespace pagx
