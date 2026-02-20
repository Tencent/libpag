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

#include "HarfBuzzShaper.h"
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include "hb.h"

namespace pagx {

template <typename T>
struct DataPointer {
  explicit DataPointer(std::shared_ptr<T> data) : data(std::move(data)) {
  }
  std::shared_ptr<T> data;
};

static hb_blob_t* GetTable(hb_face_t*, hb_tag_t tag, void* userData) {
  auto typeface = reinterpret_cast<DataPointer<tgfx::Typeface>*>(userData)->data;
  auto tableData = typeface->copyTableData(tag);
  if (tableData == nullptr) {
    return nullptr;
  }
  auto wrapper = new DataPointer<tgfx::Data>(tableData);
  return hb_blob_create(static_cast<const char*>(tableData->data()),
                        static_cast<unsigned int>(tableData->size()), HB_MEMORY_MODE_READONLY,
                        static_cast<void*>(wrapper), [](void* ctx) {
                          delete reinterpret_cast<DataPointer<tgfx::Data>*>(ctx);
                        });
}

static std::shared_ptr<hb_face_t> CreateHBFace(const std::shared_ptr<tgfx::Typeface>& typeface) {
  if (typeface == nullptr) {
    return nullptr;
  }
  auto wrapper = new DataPointer<tgfx::Typeface>(typeface);
  auto face = std::shared_ptr<hb_face_t>(
      hb_face_create_for_tables(GetTable, static_cast<void*>(wrapper),
                                [](void* ctx) {
                                  delete reinterpret_cast<DataPointer<tgfx::Typeface>*>(ctx);
                                }),
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
      : _lru(lru), _cache(cache), _mutex(mutex) {
    _mutex->lock();
  }
  FontCache(const FontCache&) = delete;
  FontCache& operator=(const FontCache&) = delete;
  FontCache& operator=(FontCache&&) = delete;

  ~FontCache() {
    _mutex->unlock();
  }

  std::shared_ptr<hb_font_t> find(uint32_t fontId) {
    auto iter = std::find(_lru->begin(), _lru->end(), fontId);
    if (iter == _lru->end()) {
      return nullptr;
    }
    _lru->erase(iter);
    _lru->push_front(fontId);
    return (*_cache)[fontId];
  }

  std::shared_ptr<hb_font_t> insert(uint32_t fontId, std::shared_ptr<hb_font_t> font) {
    if (hb_font_get_empty() == font.get()) {
      return nullptr;
    }
    _cache->insert(std::make_pair(fontId, std::move(font)));
    _lru->push_front(fontId);
    while (_lru->size() > MAX_CACHE_SIZE) {
      auto id = _lru->back();
      _lru->pop_back();
      _cache->erase(id);
    }
    return (*_cache)[fontId];
  }

  void reset() {
    _lru->clear();
    _cache->clear();
  }

 private:
  static constexpr size_t MAX_CACHE_SIZE = 100;
  std::list<uint32_t>* _lru;
  std::map<uint32_t, std::shared_ptr<hb_font_t>>* _cache;
  std::mutex* _mutex;
};

static FontCache GetFontCache() {
  static auto* cacheMutex = new std::mutex();
  static auto* cacheLRU = new std::list<uint32_t>();
  static auto* cacheMap = new std::map<uint32_t, std::shared_ptr<hb_font_t>>();
  return {cacheLRU, cacheMap, cacheMutex};
}

static std::shared_ptr<hb_font_t> CreateHBFont(
    const std::shared_ptr<tgfx::Typeface>& typeface) {
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
  auto hbFont =
      std::shared_ptr<hb_font_t>(hb_font_create(face.get()), hb_font_destroy);
  // Set scale to UPEM so output positions are in font units. The caller converts to pixels by
  // multiplying with (fontSize / UPEM). This allows caching hb_font per typeface regardless of
  // font size — a standard optimization used by Chrome and other engines.
  auto upem = static_cast<int>(typeface->unitsPerEm());
  hb_font_set_scale(hbFont.get(), upem, upem);
  return cache.insert(typeface->uniqueID(), hbFont);
}

// Shapes a single run. The run must be homogeneous in font and script.
static std::vector<ShapedGlyph> ShapeRun(const std::string& text, size_t byteOffset,
                                         size_t byteLength, const tgfx::Font& font,
                                         hb_script_t script, bool vertical, bool rtl) {
  auto typeface = font.getTypeface();
  auto hbFont = CreateHBFont(typeface);
  if (hbFont == nullptr) {
    return {};
  }

  auto buffer =
      std::shared_ptr<hb_buffer_t>(hb_buffer_create(), hb_buffer_destroy);
  if (!hb_buffer_allocation_successful(buffer.get())) {
    return {};
  }

  // Add the run substring, but provide full text as context for correct shaping at boundaries.
  hb_buffer_add_utf8(buffer.get(), text.data(), static_cast<int>(text.size()),
                     static_cast<unsigned int>(byteOffset),
                     static_cast<int>(byteLength));

  hb_feature_t features[2] = {};
  unsigned int featureCount = 0;

  if (vertical) {
    hb_buffer_set_direction(buffer.get(), HB_DIRECTION_TTB);
    hb_buffer_set_script(buffer.get(), script);
    hb_buffer_set_language(buffer.get(), hb_language_get_default());
    features[featureCount++] = {HB_TAG('v', 'e', 'r', 't'), 1, 0, static_cast<unsigned>(-1)};
    features[featureCount++] = {HB_TAG('v', 'r', 't', '2'), 1, 0, static_cast<unsigned>(-1)};
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

  auto upem = static_cast<float>(typeface->unitsPerEm());
  auto fontSize = font.getSize();
  auto scale = fontSize / upem;

  std::vector<ShapedGlyph> result;
  result.reserve(glyphCount);
  for (unsigned int i = 0; i < glyphCount; ++i) {
    ShapedGlyph glyph;
    glyph.glyphID = static_cast<tgfx::GlyphID>(infos[i].codepoint);
    glyph.cluster = infos[i].cluster;
    glyph.xAdvance = static_cast<float>(positions[i].x_advance) * scale;
    glyph.yAdvance = static_cast<float>(positions[i].y_advance) * scale;
    glyph.xOffset = static_cast<float>(positions[i].x_offset) * scale;
    glyph.yOffset = static_cast<float>(positions[i].y_offset) * scale;
    glyph.font = font;
    result.push_back(glyph);
  }
  return result;
}

// Decodes a UTF-8 sequence into a Unicode code point. Returns the number of bytes consumed.
static size_t DecodeUTF8(const char* data, size_t length, int32_t* codepoint) {
  if (length == 0) {
    *codepoint = 0;
    return 0;
  }
  auto byte = static_cast<uint8_t>(data[0]);
  if (byte < 0x80) {
    *codepoint = byte;
    return 1;
  }
  if ((byte & 0xE0) == 0xC0 && length >= 2) {
    *codepoint = ((byte & 0x1F) << 6) | (static_cast<uint8_t>(data[1]) & 0x3F);
    return 2;
  }
  if ((byte & 0xF0) == 0xE0 && length >= 3) {
    *codepoint = ((byte & 0x0F) << 12) | ((static_cast<uint8_t>(data[1]) & 0x3F) << 6) |
                 (static_cast<uint8_t>(data[2]) & 0x3F);
    return 3;
  }
  if ((byte & 0xF8) == 0xF0 && length >= 4) {
    *codepoint = ((byte & 0x07) << 18) | ((static_cast<uint8_t>(data[1]) & 0x3F) << 12) |
                 ((static_cast<uint8_t>(data[2]) & 0x3F) << 6) |
                 (static_cast<uint8_t>(data[3]) & 0x3F);
    return 4;
  }
  *codepoint = 0xFFFD;
  return 1;
}

// A shaped run: a contiguous byte range with uniform font and script.
struct ShapingRun {
  size_t byteStart = 0;
  size_t byteEnd = 0;
  tgfx::Font font = {};
  hb_script_t script = HB_SCRIPT_COMMON;
};

// Resolves the effective script for a Unicode code point. COMMON and INHERITED scripts inherit the
// previous "real" script so they stay in the same shaping run as their neighbors. This matches the
// approach used by rive-runtime, Pango, and Chrome.
static hb_script_t ResolveScript(int32_t codepoint, hb_script_t lastScript) {
  auto ufuncs = hb_unicode_funcs_get_default();
  auto script = hb_unicode_general_category(ufuncs, codepoint) ==
                        HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK
                    ? HB_SCRIPT_INHERITED
                    : hb_unicode_script(ufuncs, codepoint);
  if (script == HB_SCRIPT_COMMON || script == HB_SCRIPT_INHERITED) {
    return lastScript;
  }
  return script;
}

// Performs combined font + script itemization. Each character is assigned the best available font
// and an effective script. Adjacent characters sharing both font and script are merged into runs.
static std::vector<ShapingRun> ItemizeRuns(const std::string& text,
                                           const tgfx::Font& primaryFont,
                                           const std::vector<tgfx::Font>& fallbackFonts) {
  std::vector<ShapingRun> runs;
  size_t pos = 0;
  const tgfx::Font* lastFont = nullptr;
  hb_script_t lastScript = HB_SCRIPT_COMMON;

  while (pos < text.size()) {
    int32_t codepoint = 0;
    auto charLen = DecodeUTF8(text.data() + pos, text.size() - pos, &codepoint);
    if (charLen == 0) {
      pos++;
      continue;
    }

    // Find the best font for this codepoint.
    const tgfx::Font* bestFont = nullptr;
    if (primaryFont.getGlyphID(codepoint) != 0) {
      bestFont = &primaryFont;
    } else {
      for (const auto& fallback : fallbackFonts) {
        if (fallback.getGlyphID(codepoint) != 0) {
          bestFont = &fallback;
          break;
        }
      }
    }

    if (bestFont == nullptr) {
      bestFont = &primaryFont;
    }

    auto script = ResolveScript(codepoint, lastScript);

    bool sameFont = lastFont != nullptr && bestFont->getTypeface() == lastFont->getTypeface();
    bool sameScript = (script == lastScript);
    if (sameFont && sameScript) {
      runs.back().byteEnd = pos + charLen;
    } else {
      ShapingRun run = {};
      run.byteStart = pos;
      run.byteEnd = pos + charLen;
      run.font = *bestFont;
      run.script = script;
      runs.push_back(run);
      lastFont = bestFont;
    }

    lastScript = script;
    pos += charLen;
  }

  return runs;
}

std::vector<ShapedGlyph> HarfBuzzShaper::Shape(const std::string& text,
                                               const tgfx::Font& primaryFont,
                                               const std::vector<tgfx::Font>& fallbackFonts,
                                               bool vertical, bool rtl) {
  if (text.empty()) {
    return {};
  }

  // Phase 1: Combined font + script itemization.
  auto shapingRuns = ItemizeRuns(text, primaryFont, fallbackFonts);
  if (shapingRuns.empty()) {
    return {};
  }

  // Phase 2: Per-run shaping — shape each run independently with full text context.
  std::vector<ShapedGlyph> result;
  result.reserve(text.size());

  for (auto& run : shapingRuns) {
    auto shaped = ShapeRun(text, run.byteStart, run.byteEnd - run.byteStart, run.font,
                           run.script, vertical, rtl);
    result.insert(result.end(), shaped.begin(), shaped.end());
  }

  return result;
}

void HarfBuzzShaper::PurgeCaches() {
  auto cache = GetFontCache();
  cache.reset();
}

}  // namespace pagx

#endif
