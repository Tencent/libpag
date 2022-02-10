/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "WebTypeface.h"
#include <vector>
#include "base/utils/UniqueID.h"
#include "platform/web/NativeTextureBuffer.h"
#include "rendering/FontManager.h"

using namespace emscripten;

namespace pag {
std::shared_ptr<Typeface> Typeface::MakeFromName(const std::string& name, const std::string&) {
  return WebTypeface::Make(name);
}

std::shared_ptr<Typeface> Typeface::MakeFromPath(const std::string&, int) {
  return nullptr;
}

std::shared_ptr<Typeface> Typeface::MakeFromBytes(const void*, size_t, int) {
  return nullptr;
}

std::shared_ptr<Typeface> Typeface::MakeDefault() {
  return WebTypeface::Make("Arial");
}

std::shared_ptr<WebTypeface> WebTypeface::Make(const std::string& name) {
  auto scalerContextClass = val::module_property("ScalerContext");
  if (!scalerContextClass.as<bool>()) {
    return nullptr;
  }
  auto webTypeface = std::shared_ptr<WebTypeface>(new WebTypeface(name));
  webTypeface->scalerContextClass = scalerContextClass;
  return webTypeface;
}

WebTypeface::WebTypeface(std::string name) : _uniqueID(UniqueID::Next()), name(std::move(name)) {
}

WebTypeface::~WebTypeface() {
  delete fontMetricsMap;
}

bool WebTypeface::hasColor() const {
  auto emojiName = name;
  std::transform(emojiName.begin(), emojiName.end(), emojiName.begin(), ::tolower);
  return emojiName.find("emoji") != std::string::npos;
}

// The web side does not involve multithreading and does not require locking.
static std::unordered_map<std::string, std::vector<std::string>>& GlyphsMap() {
  static auto& glyphs = *new std::unordered_map<std::string, std::vector<std::string>>;
  return glyphs;
}

GlyphID WebTypeface::getGlyphID(const std::string& text) const {
  if (!hasColor() && scalerContextClass.call<bool>("isEmoji", text)) {
    return 0;
  }
  auto& glyphs = GlyphsMap()[name];
  auto iter = std::find(glyphs.begin(), glyphs.end(), text);
  if (iter != glyphs.end()) {
    return iter - glyphs.begin() + 1;
  }
  if (glyphs.size() >= UINT16_MAX) {
    return 0;
  }
  glyphs.push_back(text);
  return glyphs.size();
}

std::string WebTypeface::getText(GlyphID glyphID) const {
  if (glyphID == 0 || GlyphsMap().find(name) == GlyphsMap().end()) {
    return "";
  }
  const auto& glyphs = GlyphsMap()[name];
  if (glyphID > glyphs.size()) {
    return "";
  }
  return glyphs.at(glyphID - 1);
}

Point WebTypeface::getGlyphVerticalOffset(GlyphID glyphID, float size, bool fauxBold,
                                          bool fauxItalic) const {
  if (glyphID == 0) {
    return Point::Zero();
  }
  auto metrics = getMetrics(size);
  auto advance = getGlyphAdvance(glyphID, size, fauxBold, fauxItalic, true);
  return {-advance * 0.5f, metrics.capHeight};
}

float WebTypeface::getGlyphAdvance(GlyphID glyphID, float size, bool fauxBold, bool fauxItalic,
                                   bool) const {
  if (glyphID == 0) {
    return 0;
  }
  auto scalerContext = scalerContextClass.new_(name, size, fauxBold, fauxItalic);
  return scalerContext.call<float>("getTextAdvance", getText(glyphID));
}

FontMetrics WebTypeface::getMetrics(float size) const {
  auto iter = fontMetricsMap->find(size);
  if (iter != fontMetricsMap->end()) {
    return iter->second;
  }
  auto scalerContext = scalerContextClass.new_(name, size);
  auto metrics = scalerContext.call<FontMetrics>("generateFontMetrics");
  fontMetricsMap->insert(std::make_pair(size, metrics));
  return metrics;
}

bool WebTypeface::getGlyphPath(GlyphID, float, bool, bool, Path*) const {
  return false;
}

Rect WebTypeface::getGlyphBounds(GlyphID glyphID, float size, bool fauxBold,
                                 bool fauxItalic) const {
  if (glyphID == 0) {
    return Rect::MakeEmpty();
  }
  auto scalerContext = scalerContextClass.new_(name, size, fauxBold, fauxItalic);
  return scalerContext.call<Rect>("getTextBounds", getText(glyphID));
}

std::shared_ptr<TextureBuffer> WebTypeface::getGlyphImage(GlyphID glyphID, float size,
                                                          bool fauxBold, bool fauxItalic,
                                                          Matrix* matrix) const {
  if (glyphID == 0) {
    return nullptr;
  }
  auto scalerContext = scalerContextClass.new_(name, size, fauxBold, fauxItalic);
  auto bounds = scalerContext.call<Rect>("getTextBounds", getText(glyphID));
  auto buffer = scalerContext.call<val>("generateImage", getText(glyphID), bounds);
  if (matrix) {
    matrix->setTranslate(bounds.left, bounds.top);
  }
  return NativeTextureBuffer::Make(buffer.call<int>("width"), buffer.call<int>("height"), buffer);
}
}  // namespace pag
