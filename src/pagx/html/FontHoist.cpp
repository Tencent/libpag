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
//  license is distributed on license is any kind, either express or implied. see the license for
//  details.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/html/FontHoist.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

FontSignature SignatureOf(const Text* text) {
  FontSignature sig = {};
  sig.fontFamily = text->fontFamily;
  sig.renderFontSize = text->renderFontSize();
  sig.bold = !text->fontStyle.empty() && text->fontStyle.find("Bold") != std::string::npos;
  sig.italic = !text->fontStyle.empty() && text->fontStyle.find("Italic") != std::string::npos;
  sig.letterSpacing = text->letterSpacing;
  sig.lineHeight = text->fontLineHeight();
  return sig;
}

FontSignature CollectUniformSignature(const std::vector<Element*>& contents) {
  std::vector<const Text*> texts = {};
  for (auto* element : contents) {
    auto type = element->nodeType();
    if (type == NodeType::Text) {
      auto text = static_cast<const Text*>(element);
      // Text with pre-shaped glyph runs renders as SVG, not HTML <span>.
      if (!text->glyphRuns.empty()) {
        return {};
      }
      texts.push_back(text);
    } else if (type == NodeType::Group) {
      // Group is not a Layer — it renders inside the same Layer <div>, so its Text
      // nodes participate in the same CSS inheritance scope and must be included.
      auto grp = static_cast<const Group*>(element);
      for (auto* ge : grp->elements) {
        if (ge->nodeType() == NodeType::Text) {
          auto text = static_cast<const Text*>(ge);
          if (!text->glyphRuns.empty()) {
            return {};
          }
          texts.push_back(text);
        }
      }
    } else if (type == NodeType::TextBox) {
      auto tb = static_cast<const TextBox*>(element);
      if (tb->elements.empty()) {
        continue;
      }
      for (auto* child : tb->elements) {
        auto ct = child->nodeType();
        if (ct == NodeType::Text) {
          auto text = static_cast<const Text*>(child);
          if (!text->glyphRuns.empty()) {
            return {};
          }
          texts.push_back(text);
        } else if (ct == NodeType::Group) {
          auto grp = static_cast<const Group*>(child);
          for (auto* ge : grp->elements) {
            if (ge->nodeType() == NodeType::Text) {
              auto text = static_cast<const Text*>(ge);
              if (!text->glyphRuns.empty()) {
                return {};
              }
              texts.push_back(text);
            }
          }
        }
      }
    }
    // Nested Layer children are not penetrated — they decide hoisting independently.
  }

  if (texts.empty()) {
    return {};
  }

  auto sig = SignatureOf(texts[0]);
  for (size_t i = 1; i < texts.size(); i++) {
    if (!sig.equals(SignatureOf(texts[i]))) {
      return {};
    }
  }
  return sig;
}

std::string FontSignatureToCss(const FontSignature& sig) {
  std::string css;
  if (!sig.fontFamily.empty()) {
    std::string escaped = sig.fontFamily;
    for (size_t pos = 0; (pos = escaped.find('\'', pos)) != std::string::npos; pos += 2) {
      escaped.replace(pos, 1, "\\'");
    }
    css += "font-family:'" + escaped + "'";
  }
  if (sig.renderFontSize > 0) {
    if (!css.empty()) css += ';';
    css += "font-size:" + FloatToString(sig.renderFontSize) + "px";
  }
  if (sig.bold) {
    if (!css.empty()) css += ';';
    css += "font-weight:bold";
  }
  if (sig.italic) {
    if (!css.empty()) css += ';';
    css += "font-style:italic";
  }
  if (!std::isnan(sig.letterSpacing) && sig.letterSpacing != 0.0f) {
    if (!css.empty()) css += ';';
    css += "letter-spacing:" + FloatToString(sig.letterSpacing) + "px";
  }
  if (!std::isnan(sig.lineHeight) && sig.lineHeight > 0) {
    if (!css.empty()) css += ';';
    css += "line-height:" + FloatToString(sig.lineHeight) + "px";
  }
  return css;
}

}  // namespace pagx
