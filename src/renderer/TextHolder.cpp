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

#include "renderer/TextHolder.h"
#include <variant>
#include "pagx/LayoutContext.h"
#include "pagx/runtime/MixUtils.h"
#include "renderer/GlyphRunRenderer.h"
#include "renderer/LayerBuilder.h"

namespace pagx {

void TextHolder::addEntry(Text* node, RuntimeTarget* target, const TextGlyphParams& glyph,
                          const tgfx::Matrix& inverseMatrix, float paddingLeft, float paddingTop) {
  if (node == nullptr || target == nullptr) {
    return;
  }
  TextHolderEntry entry = {};
  entry.node = node;
  entry.target = target;
  entry.glyph = glyph;
  entry.inverseMatrix = inverseMatrix;
  entry.paddingLeft = paddingLeft;
  entry.paddingTop = paddingTop;
  entries.push_back(std::move(entry));
}

TextHolderEntry* TextHolder::findEntry(Text* node) {
  for (auto& entry : entries) {
    if (entry.node == node) {
      return &entry;
    }
  }
  return nullptr;
}

const TextHolderEntry* TextHolder::findEntry(Text* node) const {
  for (const auto& entry : entries) {
    if (entry.node == node) {
      return &entry;
    }
  }
  return nullptr;
}

void TextHolder::apply(Text* node, const std::string& channel, const KeyValue& value, float mix) {
  auto* entry = findEntry(node);
  if (entry == nullptr) {
    return;
  }
  auto& glyph = entry->glyph;
  // Continuous attributes mix from the current value (the role a tgfx object plays for other
  // channels); discrete attributes ignore mix and overwrite, matching the existing writer
  // convention (WriteLayerBlendMode / WriteLayerName). Only mark dirty when the value actually
  // changes: Hold-interpolated animation channels re-apply the same value every advance, so a
  // blind dirty flag would trigger a full reshape at every draw.
  if (channel == "fontSize") {
    const auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto mixed = MixFloat(glyph.fontSize, *v, mix);
    if (mixed == glyph.fontSize) {
      return;
    }
    glyph.fontSize = mixed;
  } else if (channel == "letterSpacing") {
    const auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto mixed = MixFloat(glyph.letterSpacing, *v, mix);
    if (mixed == glyph.letterSpacing) {
      return;
    }
    glyph.letterSpacing = mixed;
  } else if (channel == "text") {
    const auto* v = std::get_if<std::string>(&value);
    if (v == nullptr || *v == glyph.text) {
      return;
    }
    glyph.text = *v;
  } else if (channel == "fontFamily") {
    const auto* v = std::get_if<std::string>(&value);
    if (v == nullptr || *v == glyph.fontFamily) {
      return;
    }
    glyph.fontFamily = *v;
  } else if (channel == "fontStyle") {
    const auto* v = std::get_if<std::string>(&value);
    if (v == nullptr || *v == glyph.fontStyle) {
      return;
    }
    glyph.fontStyle = *v;
  } else if (channel == "fauxBold") {
    const auto* v = std::get_if<bool>(&value);
    if (v == nullptr || *v == glyph.fauxBold) {
      return;
    }
    glyph.fauxBold = *v;
  } else if (channel == "fauxItalic") {
    const auto* v = std::get_if<bool>(&value);
    if (v == nullptr || *v == glyph.fauxItalic) {
      return;
    }
    glyph.fauxItalic = *v;
  } else {
    return;
  }
  dirty = true;
}

bool TextHolder::read(Text* node, const std::string& channel, KeyValue* out) const {
  if (out == nullptr) {
    return false;
  }
  const auto* entry = findEntry(node);
  if (entry == nullptr) {
    return false;
  }
  const auto& glyph = entry->glyph;
  if (channel == "text") {
    *out = KeyValue{glyph.text};
  } else if (channel == "fontFamily") {
    *out = KeyValue{glyph.fontFamily};
  } else if (channel == "fontStyle") {
    *out = KeyValue{glyph.fontStyle};
  } else if (channel == "fontSize") {
    *out = KeyValue{glyph.fontSize};
  } else if (channel == "letterSpacing") {
    *out = KeyValue{glyph.letterSpacing};
  } else if (channel == "fauxBold") {
    *out = KeyValue{glyph.fauxBold};
  } else if (channel == "fauxItalic") {
    *out = KeyValue{glyph.fauxItalic};
  } else {
    return false;
  }
  return true;
}

void TextHolder::flush(FontConfig* fontConfig) {
  if (!dirty) {
    return;
  }
  dirty = false;
  std::vector<TextElement> elements = {};
  elements.reserve(entries.size());
  for (const auto& entry : entries) {
    elements.push_back({entry.node, entry.glyph});
  }
  LayoutContext context(fontConfig);
  auto result = TextLayout::Layout(elements, params, &context);
  for (auto& entry : entries) {
    auto runs = result.extractLayoutRuns(entry.node);
    // Bake the container padding offset into the run positions the same way TextBox::updateLayout
    // does, so a reshaped TextBox child stays aligned with its box inset.
    ApplyPaddingToRuns(runs, entry.paddingLeft, entry.paddingTop);
    auto textBlob = GlyphRunRenderer::BuildTextBlobFromLayoutRuns(runs, entry.inverseMatrix);
    if (textBlob == nullptr) {
      continue;
    }
    // Reshape in place: replace the existing tgfx::Text's blob rather than building a new object.
    // The render tree (parent VectorGroup / VectorLayer) and the binding both hold the same Text
    // object, so an in-place blob swap takes effect on the next draw without any reference sync.
    // The runtime position set by x / y writers is preserved because the object is not replaced.
    auto text = entry.target->getObject<tgfx::Text>();
    if (text == nullptr) {
      continue;
    }
    text->setTextBlob(textBlob);
  }
}

bool TextHolder::removeNode(const Node* node) {
  if (node == nullptr) {
    return entries.empty();
  }
  size_t writeIndex = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    if (static_cast<const Node*>(entries[i].node) == node) {
      continue;
    }
    if (writeIndex != i) {
      entries[writeIndex] = std::move(entries[i]);
    }
    ++writeIndex;
  }
  if (writeIndex < entries.size()) {
    entries.resize(writeIndex);
  }
  return entries.empty();
}

}  // namespace pagx
