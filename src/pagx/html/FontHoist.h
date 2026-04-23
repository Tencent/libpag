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

#include <cmath>
#include <string>
#include <vector>

namespace pagx {

class Text;
class TextBox;
class Element;

/**
 * Captures the font-related attributes of a Text node that are safe to hoist to a parent Layer
 * via CSS inheritance. Two Text nodes with the same FontSignature will produce identical
 * font-family / font-size / font-weight / font-style / letter-spacing CSS declarations, so
 * those declarations can be placed on the parent Layer once and inherited by every child span.
 */
struct FontSignature {
  std::string fontFamily = {};
  float renderFontSize = 0.0f;
  bool bold = false;
  bool italic = false;
  float letterSpacing = std::nanf("");
  float lineHeight = std::nanf("");

  bool equals(const FontSignature& other) const {
    if (fontFamily != other.fontFamily) return false;
    if (renderFontSize != other.renderFontSize) return false;
    if (bold != other.bold) return false;
    if (italic != other.italic) return false;
    if (!FloatEqNaN(letterSpacing, other.letterSpacing)) return false;
    if (!FloatEqNaN(lineHeight, other.lineHeight)) return false;
    return true;
  }

 private:
  static bool FloatEqNaN(float a, float b) {
    return (std::isnan(a) && std::isnan(b)) || a == b;
  }
};

/**
 * Extracts the FontSignature from a Text node.
 */
FontSignature SignatureOf(const Text* text);

/**
 * Scans a Layer's direct children for Text nodes that render as HTML <span> elements and
 * returns their uniform FontSignature if all of them share the same signature. Returns an
 * empty (default-constructed) FontSignature when the children have no Text nodes, when the
 * signatures differ, or when any Text renders as SVG instead of HTML.
 *
 * Nested Layers are not penetrated (they decide hoisting independently). Group and TextBox
 * children are penetrated — their inner Text elements participate in the signature comparison.
 */
FontSignature CollectUniformSignature(const std::vector<Element*>& contents);

/**
 * Converts a FontSignature into a CSS declaration string (e.g. "font-family:'Arial';font-size:16px").
 * Properties with default values (fontWeight 400, non-italic, letterSpacing 0, NaN lineHeight) are
 * omitted. The output order is fixed for deterministic deduplication in Phase 2.
 */
std::string FontSignatureToCss(const FontSignature& sig);

}  // namespace pagx
