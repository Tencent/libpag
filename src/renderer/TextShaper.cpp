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

#ifdef PAG_USE_HARFBUZZ

#include "TextShaper.h"
#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "TextUnicode.h"
#include "UTF8Utils.h"
#include "VerticalTextUtils.h"
#include "hb.h"
#include "pagx/LayoutContext.h"
#include "pagx/utils/CSSFontStyle.h"
#ifdef __APPLE__
#include "tgfx/platform/apple/CTTypeface.h"
#endif

namespace pagx {

template <typename T>
struct DataPointer {
  explicit DataPointer(std::shared_ptr<T> data) : data(std::move(data)) {
  }
  std::shared_ptr<T> data;
};

static void DestroyDataPointerData(void* ctx) {
  delete reinterpret_cast<DataPointer<tgfx::Data>*>(ctx);
}

static void DestroyDataPointerTypeface(void* ctx) {
  delete reinterpret_cast<DataPointer<tgfx::Typeface>*>(ctx);
}

static hb_blob_t* GetTable(hb_face_t*, hb_tag_t tag, void* userData) {
  auto typeface = reinterpret_cast<DataPointer<tgfx::Typeface>*>(userData)->data;
  auto tableData = typeface->copyTableData(tag);
  if (tableData == nullptr) {
    return nullptr;
  }
  auto wrapper = new DataPointer<tgfx::Data>(tableData);
  return hb_blob_create(static_cast<const char*>(tableData->data()),
                        static_cast<unsigned int>(tableData->size()), HB_MEMORY_MODE_READONLY,
                        static_cast<void*>(wrapper), DestroyDataPointerData);
}

static std::shared_ptr<hb_face_t> CreateHBFace(const std::shared_ptr<tgfx::Typeface>& typeface) {
  if (typeface == nullptr) {
    return nullptr;
  }
  auto wrapper = new DataPointer<tgfx::Typeface>(typeface);
  auto face = std::shared_ptr<hb_face_t>(
      hb_face_create_for_tables(GetTable, static_cast<void*>(wrapper), DestroyDataPointerTypeface),
      hb_face_destroy);
  if (hb_face_get_empty() == face.get()) {
    return nullptr;
  }
  hb_face_set_index(face.get(), 0);
  hb_face_set_upem(face.get(), typeface->unitsPerEm());
  return face;
}

class FontCache {
 public:
  FontCache(std::list<uint32_t>* lru, std::map<uint32_t, std::shared_ptr<hb_font_t>>* cache,
            std::mutex* mutex)
      : lru(lru), fontCache(cache), fontMutex(mutex) {
    fontMutex->lock();
  }
  FontCache(const FontCache&) = delete;
  FontCache(FontCache&&) = delete;
  FontCache& operator=(const FontCache&) = delete;
  FontCache& operator=(FontCache&&) = delete;

  ~FontCache() {
    fontMutex->unlock();
  }

  std::shared_ptr<hb_font_t> find(uint32_t fontId) {
    auto iter = std::find(lru->begin(), lru->end(), fontId);
    if (iter == lru->end()) {
      return nullptr;
    }
    lru->erase(iter);
    lru->push_front(fontId);
    return (*fontCache)[fontId];
  }

  std::shared_ptr<hb_font_t> insert(uint32_t fontId, std::shared_ptr<hb_font_t> font) {
    if (hb_font_get_empty() == font.get()) {
      return nullptr;
    }
    fontCache->insert_or_assign(fontId, std::move(font));
    lru->push_front(fontId);
    while (lru->size() > MAX_CACHE_SIZE) {
      auto id = lru->back();
      lru->pop_back();
      fontCache->erase(id);
    }
    return (*fontCache)[fontId];
  }

  void reset() {
    lru->clear();
    fontCache->clear();
  }

 private:
  static constexpr size_t MAX_CACHE_SIZE = 100;
  std::list<uint32_t>* lru;
  std::map<uint32_t, std::shared_ptr<hb_font_t>>* fontCache;
  std::mutex* fontMutex;
};

static FontCache GetFontCache() {
  static auto* cacheMutex = new std::mutex();
  static auto* cacheLRU = new std::list<uint32_t>();
  static auto* cacheMap = new std::map<uint32_t, std::shared_ptr<hb_font_t>>();
  return {cacheLRU, cacheMap, cacheMutex};
}

// CSS needs a stable em-box origin when a font has no vertical metrics. HarfBuzz's
// default fallback centers each glyph's ink independently, moving punctuation toward the
// middle of its cell. Keep the normal shaping offsets but synthesize a common baseline.
static hb_bool_t SynthesizedVerticalOrigin(hb_font_t* font, void*, hb_codepoint_t glyph,
                                           hb_position_t* x, hb_position_t* y, void*) {
  auto* parent = hb_font_get_parent(font);
  hb_font_extents_t extents = {};
  hb_font_get_h_extents(parent, &extents);
  *x = hb_font_get_glyph_h_advance(parent, glyph) / 2;
  *y = extents.ascender;
  return true;
}

static hb_position_t SynthesizedVerticalAdvance(hb_font_t* font, void*, hb_codepoint_t, void*) {
  int xScale = 0, yScale = 0;
  hb_font_get_scale(font, &xScale, &yScale);
  return -yScale;
}

static std::shared_ptr<hb_font_t> CreateHBFont(const std::shared_ptr<tgfx::Typeface>& typeface) {
  if (typeface == nullptr) {
    return nullptr;
  }
  auto cache = GetFontCache();
  auto font = cache.find(typeface->uniqueID());
  if (font != nullptr) {
    return font;
  }
  auto face = CreateHBFace(typeface);
  if (face == nullptr) {
    return nullptr;
  }
  auto hbFont = std::shared_ptr<hb_font_t>(hb_font_create(face.get()), hb_font_destroy);
  // Set scale to UPEM so output positions are in font units. The caller converts to pixels by
  // multiplying with (fontSize / UPEM). This allows caching hb_font per typeface regardless of
  // font size — a standard optimization used by Chrome and other engines.
  auto upem = static_cast<int>(typeface->unitsPerEm());
  hb_font_set_scale(hbFont.get(), upem, upem);
#ifdef __APPLE__
  if (auto native = tgfx::CTTypeface::GetCTFont(typeface.get())) {
    if (auto variations = CTFontCopyVariation(native)) {
      auto count = CFDictionaryGetCount(variations);
      std::vector<const void*> keys(count), values(count);
      CFDictionaryGetKeysAndValues(variations, keys.data(), values.data());
      std::vector<hb_variation_t> axes;
      for (CFIndex i = 0; i < count; ++i) {
        int tag = 0;
        float value = 0;
        CFNumberGetValue(static_cast<CFNumberRef>(keys[i]), kCFNumberIntType, &tag);
        CFNumberGetValue(static_cast<CFNumberRef>(values[i]), kCFNumberFloatType, &value);
        axes.push_back({static_cast<hb_tag_t>(tag), value});
      }
      hb_font_set_variations(hbFont.get(), axes.data(), static_cast<unsigned>(axes.size()));
      CFRelease(variations);
    }
  }
#endif
  if (!typeface->copyTableData(HB_TAG('v', 'h', 'e', 'a')) ||
      !typeface->copyTableData(HB_TAG('v', 'm', 't', 'x'))) {
    auto child = std::shared_ptr<hb_font_t>(hb_font_create_sub_font(hbFont.get()), hb_font_destroy);
    auto* funcs = hb_font_funcs_create();
    hb_font_funcs_set_glyph_v_origin_func(funcs, SynthesizedVerticalOrigin, nullptr, nullptr);
    hb_font_funcs_set_glyph_v_advance_func(funcs, SynthesizedVerticalAdvance, nullptr, nullptr);
    hb_font_set_funcs(child.get(), funcs, nullptr, nullptr);
    hb_font_funcs_destroy(funcs);
    hbFont = std::move(child);
  }
  return cache.insert(typeface->uniqueID(), hbFont);
}

#ifdef __APPLE__
struct NativeFontMetrics {
  const tgfx::Font* font = nullptr;
  float toDesignUnits = 1;
};

static hb_position_t NativeAdvance(hb_font_t*, void* data, hb_codepoint_t glyph, void*) {
  const auto& metrics = *static_cast<NativeFontMetrics*>(data);
  return static_cast<hb_position_t>(
      std::round(metrics.font->getAdvance(glyph) * metrics.toDesignUnits));
}
static hb_position_t NativeVerticalAdvance(hb_font_t*, void* data, hb_codepoint_t glyph, void*) {
  const auto& metrics = *static_cast<NativeFontMetrics*>(data);
  return static_cast<hb_position_t>(
      -std::round(metrics.font->getAdvance(glyph, true) * metrics.toDesignUnits));
}
static hb_bool_t NativeExtents(hb_font_t*, void* data, hb_font_extents_t* extents, void*) {
  const auto& native = *static_cast<NativeFontMetrics*>(data);
  auto metrics = native.font->getMetrics();
  extents->ascender =
      static_cast<hb_position_t>(std::round(-metrics.ascent * native.toDesignUnits));
  extents->descender =
      static_cast<hb_position_t>(std::round(-metrics.descent * native.toDesignUnits));
  extents->line_gap =
      static_cast<hb_position_t>(std::round(metrics.leading * native.toDesignUnits));
  return true;
}
#endif

// Shapes a single run. The run must be homogeneous in font and script.
static std::vector<ShapedGlyph> ShapeRun(const std::string& text, size_t byteOffset,
                                         size_t byteLength, const tgfx::Font& font,
                                         hb_script_t script, bool vertical, bool rtl,
                                         bool isolated = false) {
  auto typeface = font.getTypeface();
  auto hbFont = CreateHBFont(typeface);
  if (hbFont == nullptr) {
    return {};
  }
  float scale = font.getSize() / std::max(1, typeface->unitsPerEm());
#ifdef __APPLE__
  NativeFontMetrics native = {};
  if (tgfx::CTTypeface::GetCTFont(typeface.get()) && font.getSize() > 0) {
    // Keep fractional advances in design space. Rounding each glyph to 1/64 pixel changes
    // ordinary font spacing and accumulates along a line, especially in small text.
    constexpr int nativeUnitsPerEm = 65536;
    native = {&font, nativeUnitsPerEm / font.getSize()};
    auto child = std::shared_ptr<hb_font_t>(hb_font_create_sub_font(hbFont.get()), hb_font_destroy);
    auto* funcs = hb_font_funcs_create();
    hb_font_funcs_set_glyph_h_advance_func(funcs, NativeAdvance, nullptr, nullptr);
    hb_font_funcs_set_glyph_v_advance_func(funcs, NativeVerticalAdvance, nullptr, nullptr);
    hb_font_funcs_set_font_h_extents_func(funcs, NativeExtents, nullptr, nullptr);
    hb_font_set_funcs(child.get(), funcs, &native, nullptr);
    hb_font_funcs_destroy(funcs);
    hb_font_set_scale(child.get(), nativeUnitsPerEm, nativeUnitsPerEm);
    hbFont = std::move(child);
    scale = font.getSize() / nativeUnitsPerEm;
  }
#endif

  auto buffer = std::shared_ptr<hb_buffer_t>(hb_buffer_create(), hb_buffer_destroy);
  if (!hb_buffer_allocation_successful(buffer.get())) {
    return {};
  }

  // Add the run substring, but provide full text as context for correct shaping at boundaries.
  if (isolated) {
    hb_buffer_add_utf8(buffer.get(), text.data() + byteOffset, static_cast<int>(byteLength), 0,
                       static_cast<int>(byteLength));
  } else {
    hb_buffer_add_utf8(buffer.get(), text.data(), static_cast<int>(text.size()),
                       static_cast<unsigned int>(byteOffset), static_cast<int>(byteLength));
  }

  hb_feature_t features[2] = {};
  unsigned int featureCount = 0;

  if (vertical) {
    hb_buffer_set_direction(buffer.get(), HB_DIRECTION_TTB);
    hb_buffer_set_script(buffer.get(), script);
    hb_buffer_set_language(buffer.get(), hb_language_get_default());
    features[featureCount++] = {HB_TAG('v', 'e', 'r', 't'), 1, 0, static_cast<unsigned>(-1)};
    features[featureCount++] = {HB_TAG('v', 'r', 't', '2'), 0, 0, static_cast<unsigned>(-1)};
  } else if (rtl) {
    hb_buffer_set_direction(buffer.get(), HB_DIRECTION_RTL);
    hb_buffer_set_script(buffer.get(), script);
    hb_buffer_set_language(buffer.get(), hb_language_get_default());
  } else {
    hb_buffer_set_direction(buffer.get(), HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer.get(), script);
    hb_buffer_set_language(buffer.get(), hb_language_get_default());
  }

  hb_shape(hbFont.get(), buffer.get(), features, featureCount);

  unsigned int glyphCount = 0;
  auto* infos = hb_buffer_get_glyph_infos(buffer.get(), &glyphCount);
  auto* positions = hb_buffer_get_glyph_positions(buffer.get(), &glyphCount);

  std::vector<ShapedGlyph> result;
  result.reserve(glyphCount);
  for (unsigned int i = 0; i < glyphCount; ++i) {
    ShapedGlyph glyph = {};
    glyph.glyphID = static_cast<tgfx::GlyphID>(infos[i].codepoint);
    glyph.cluster = infos[i].cluster + (isolated ? static_cast<uint32_t>(byteOffset) : 0);
    glyph.xAdvance = static_cast<float>(positions[i].x_advance) * scale;
    glyph.yAdvance = static_cast<float>(positions[i].y_advance) * scale;
    glyph.xOffset = static_cast<float>(positions[i].x_offset) * scale;
    glyph.yOffset = static_cast<float>(positions[i].y_offset) * scale;
    glyph.font = font;
    result.push_back(glyph);
  }
  return result;
}

// A shaped run: a contiguous byte range with uniform font and script.
struct ShapingRun {
  size_t byteStart = 0;
  size_t byteEnd = 0;
  tgfx::Font font = {};
  hb_script_t script = HB_SCRIPT_COMMON;
  bool vertical = false;
  bool isolated = false;
};

// Resolves the effective script for a Unicode code point. COMMON and INHERITED scripts inherit the
// previous "real" script so they stay in the same shaping run as their neighbors. This matches the
// approach used by rive-runtime, Pango, and Chrome.
static hb_script_t ResolveScript(int32_t codepoint, hb_script_t lastScript) {
  auto ufuncs = hb_unicode_funcs_get_default();
  auto script =
      hb_unicode_general_category(ufuncs, codepoint) == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK
          ? HB_SCRIPT_INHERITED
          : hb_unicode_script(ufuncs, codepoint);
  if (script == HB_SCRIPT_COMMON || script == HB_SCRIPT_INHERITED) {
    return lastScript;
  }
  return script;
}

enum class Presentation { Any, Text, Emoji };

static Presentation ClusterPresentation(const TextCluster& cluster, bool useUnicodeDefault = true) {
  bool emoji = false;
  bool emojiDefault = false;
  for (size_t i = 0; i < cluster.codepoints.size(); ++i) {
    auto cp = cluster.codepoints[i];
    if (i > 0 && IsEmojiVariationBase(cluster.codepoints[i - 1])) {
      if (cp == 0xFE0E) return Presentation::Text;
      if (cp == 0xFE0F) return Presentation::Emoji;
    }
    emoji |= IsEmojiCharacter(cp);
    emojiDefault |= IsEmojiPresentation(cp);
  }
  if (!useUnicodeDefault) return Presentation::Any;
  if (!emoji) return Presentation::Any;
  return emojiDefault || cluster.codepoints.back() == 0x20E3 ? Presentation::Emoji
                                                             : Presentation::Text;
}

struct PresentationText {
  std::string text;
  std::vector<size_t> sourceOffsets;
};

// Apply CSS's implicit selectors only to Unicode emoji variation bases. Shape the resulting
// sequence, not just a different font: one face can contain both presentations in cmap format 14.
// Keep a byte map so implicit selectors never change the public source-cluster offsets.
static PresentationText ApplyEmojiPresentation(const std::string& text, const std::string& style) {
  PresentationText result;
  if (style != "text" && style != "emoji" && style != "unicode") return result;
  for (size_t offset = 0; offset < text.size();) {
    int32_t codepoint = 0;
    auto length = DecodeUTF8(text.data() + offset, text.size() - offset, &codepoint);
    if (!length) length = 1;
    result.text.append(text, offset, length);
    for (size_t i = 0; i < length; ++i) result.sourceOffsets.push_back(offset + i);
    int32_t next = 0;
    if (offset + length < text.size()) {
      DecodeUTF8(text.data() + offset + length, text.size() - offset - length, &next);
    }
    if (IsEmojiVariationBase(codepoint) && next != 0xFE0E && next != 0xFE0F) {
      bool emoji = style == "emoji" || (style == "unicode" && IsEmojiPresentation(codepoint));
      result.text += emoji ? "\xEF\xB8\x8F" : "\xEF\xB8\x8E";
      result.sourceOffsets.insert(result.sourceOffsets.end(), 3, offset);
    }
    offset += length;
  }
  return result;
}

// Probe the whole cluster. Default-ignorables do not need standalone glyphs, but an emoji joiner
// that survives shaping indicates an unsupported sequence. A successful cluster may contain more
// than one glyph (marks/layered representations); glyph count is not a coverage test.
static bool SupportsCluster(const std::shared_ptr<tgfx::Typeface>& typeface,
                            const std::string& text, const TextCluster& cluster, hb_script_t script,
                            Presentation presentation, bool strict) {
  auto font = CreateHBFont(typeface);
  if (!font) return false;
  bool variant = false;
  for (size_t i = 1; i < cluster.codepoints.size(); ++i) {
    auto cp = cluster.codepoints[i];
    if (cp == 0xFE0E || cp == 0xFE0F) {
      hb_codepoint_t glyph = 0;
      variant |= hb_font_get_variation_glyph(font.get(), cluster.codepoints[i - 1], cp, &glyph);
    }
  }
  // Color is a presentation preference, not proof of sequence support. Explicit UVS mappings
  // can supply either presentation in an outline font. If no preferred face works, retry with
  // ordinary cluster matching so monochrome emoji and unsupported presentations still display.
  if (strict && !variant) {
    if (presentation == Presentation::Emoji && !typeface->hasColor()) return false;
    if (presentation == Presentation::Text && typeface->hasColor()) return false;
  }
  auto buffer = std::shared_ptr<hb_buffer_t>(hb_buffer_create(), hb_buffer_destroy);
  hb_buffer_add_utf8(buffer.get(), text.data(), static_cast<int>(text.size()),
                     static_cast<unsigned>(cluster.start),
                     static_cast<int>(cluster.end - cluster.start));
  hb_buffer_set_direction(buffer.get(), hb_script_get_horizontal_direction(script));
  hb_buffer_set_script(buffer.get(), script);
  hb_buffer_set_language(buffer.get(), hb_language_get_default());
  hb_buffer_set_cluster_level(buffer.get(), HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
  hb_buffer_set_flags(buffer.get(), HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES);
  hb_shape(font.get(), buffer.get(), nullptr, 0);
  unsigned count = 0;
  auto* glyphs = hb_buffer_get_glyph_infos(buffer.get(), &count);
  auto* positions = hb_buffer_get_glyph_positions(buffer.get(), nullptr);
  auto sequencePresentation = ClusterPresentation(cluster);
  bool regionalPair = cluster.codepoints.size() == 2 &&
                      IsRegionalIndicator(cluster.codepoints.front()) &&
                      IsRegionalIndicator(cluster.codepoints.back());
  for (unsigned i = 0; i < count; ++i) {
    auto offset = glyphs[i].cluster;
    int32_t cp = 0;
    if (offset < text.size()) DecodeUTF8(text.data() + offset, text.size() - offset, &cp);
    bool ignorable = IsDefaultIgnorable(cp);
    if (!glyphs[i].codepoint && !ignorable) return false;
    if (strict && sequencePresentation == Presentation::Emoji &&
        (cp == 0x200D || (cp >= 0xE0020 && cp <= 0xE007F)))
      return false;
    // A separately advancing modifier/second regional indicator is an uncombined component.
    // Zero-advance marks and multiple glyphs sharing the base cluster remain valid.
    if (strict && sequencePresentation == Presentation::Emoji && offset > cluster.start &&
        (IsEmojiModifier(cp) || cp == 0x20E3 || (regionalPair && IsRegionalIndicator(cp))) &&
        (positions[i].x_advance != 0 || positions[i].y_advance != 0))
      return false;
  }
  return count != 0;
}

// Resolve complete grapheme clusters, then merge compatible clusters into contextual shaping runs.
static std::vector<ShapingRun> ItemizeRuns(const std::string& text, const tgfx::Font& primaryFont,
                                           LayoutContext& layoutContext, bool vertical,
                                           TextOrientation orientation, const std::string& family,
                                           const std::string& style,
                                           const std::string& familyList) {
  std::vector<ShapingRun> runs;
  auto families = familyList.empty() ? std::vector<std::string>{family}
                                     : ParseCSSFontFamilyList(familyList, true);
  hb_script_t lastScript = HB_SCRIPT_COMMON;
  auto primaryTypeface = primaryFont.getTypeface();
  std::unordered_map<std::string, std::shared_ptr<tgfx::Typeface>> resolvedClusters;
  for (const auto& cluster : SegmentTextClusters(text)) {
    auto codepoint = cluster.codepoints.front();
    auto script = ResolveScript(codepoint, lastScript);
    auto key = text.substr(cluster.start, cluster.end - cluster.start);
    auto found = resolvedClusters.find(key);
    std::shared_ptr<tgfx::Typeface> selected;
    if (found != resolvedClusters.end()) {
      selected = found->second;
    } else {
      // Unicode defaults guide system fallback, but must not reject a covering CSS family.
      // In particular, digits, # and * in a display color font are still ordinary text.
      // Explicit selectors (including those inserted for font-variant-emoji) do request a
      // presentation and retain priority over the family's default glyphs.
      auto presentation = ClusterPresentation(cluster, false);
      for (bool strict : {true, false}) {
        auto accepts = [&](const auto& typeface) {
          return SupportsCluster(typeface, text, cluster, script, presentation, strict);
        };
        for (const auto& name : families) {
          if (name.empty()) {
            if (primaryTypeface && accepts(primaryTypeface)) selected = primaryTypeface;
          } else {
            selected = layoutContext.findClusterTypeface(name, style, primaryFont.getSize(),
                                                         cluster.codepoints, accepts);
          }
          if (selected) break;
        }
        if (!selected) {
          auto fallbackAccepts = [&](const auto& typeface) {
            return SupportsCluster(typeface, text, cluster, script, ClusterPresentation(cluster),
                                   strict);
          };
          selected = layoutContext.fallbackTypeface(fallbackAccepts, primaryFont.getSize());
        }
        if (selected) break;
      }
      if (!selected) selected = primaryTypeface;
      resolvedClusters.emplace(std::move(key), selected);
    }
    tgfx::Font font(selected, primaryFont.getSize());
    font.setFauxBold(primaryFont.isFauxBold());
    font.setFauxItalic(primaryFont.isFauxItalic());
    bool upright = vertical &&
                   (orientation == TextOrientation::Upright ||
                    (orientation == TextOrientation::Mixed &&
                     VerticalTextUtils::GetOrientation(codepoint) == VerticalOrientation::Upright));
    bool isolated = upright && hb_script_get_horizontal_direction(script) == HB_DIRECTION_RTL;
    if (!runs.empty() && runs.back().font.getTypeface() == selected &&
        runs.back().script == script && runs.back().vertical == upright && !isolated) {
      runs.back().byteEnd = cluster.end;
    } else {
      runs.push_back({cluster.start, cluster.end, font, script, upright, isolated});
    }
    lastScript = script;
  }
  return runs;
}

std::vector<ShapedGlyph> TextShaper::Shape(const std::string& text, const tgfx::Font& primaryFont,
                                           LayoutContext& layoutContext, bool vertical, bool rtl,
                                           TextOrientation orientation, const std::string& family,
                                           const std::string& style, const std::string& familyList,
                                           const std::string& emojiStyle) {
  if (text.empty()) {
    return {};
  }

  // Phase 1: Combined font + script itemization.
  auto presentation = ApplyEmojiPresentation(text, emojiStyle);
  const auto& shapingText = presentation.sourceOffsets.empty() ? text : presentation.text;
  auto shapingRuns = ItemizeRuns(shapingText, primaryFont, layoutContext, vertical, orientation,
                                 family, style, familyList);
  if (shapingRuns.empty()) {
    return {};
  }

  // Phase 2: Per-run shaping — shape each run independently with full text context.
  std::vector<ShapedGlyph> result;
  result.reserve(text.size() / 2 + 1);

  if (rtl && (!vertical || orientation != TextOrientation::Upright)) {
    std::reverse(shapingRuns.begin(), shapingRuns.end());
  }
  for (auto& run : shapingRuns) {
    auto shaped = ShapeRun(shapingText, run.byteStart, run.byteEnd - run.byteStart, run.font,
                           run.script, run.vertical, rtl, run.isolated);
    for (auto& glyph : shaped) {
      glyph.sideways = vertical && !run.vertical;
      if (!presentation.sourceOffsets.empty()) {
        glyph.cluster = static_cast<uint32_t>(presentation.sourceOffsets[glyph.cluster]);
      }
    }
    result.insert(result.end(), shaped.begin(), shaped.end());
  }

  return result;
}

void TextShaper::PurgeCaches() {
  auto cache = GetFontCache();
  cache.reset();
}

}  // namespace pagx

#endif
